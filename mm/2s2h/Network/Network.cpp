#ifdef ENABLE_ANCHOR
#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include "Network.h"
#include <algorithm>

namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket INVALID = INVALID_SOCKET;
void Close(Socket socket) {
    closesocket(socket);
}
bool Pending() {
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
}
bool Nonblocking(Socket socket) {
    u_long on = 1;
    return ioctlsocket(socket, FIONBIO, &on) == 0;
}
#else
using Socket = int;
constexpr Socket INVALID = -1;
void Close(Socket socket) {
    close(socket);
}
bool Pending() {
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS || errno == EINTR;
}
bool Nonblocking(Socket socket) {
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif
int Poll(Socket socket, bool write) {
#ifndef _WIN32
    if (socket < 0 || socket >= FD_SETSIZE)
        return -1;
#endif
    fd_set ready;
    FD_ZERO(&ready);
    FD_SET(socket, &ready);
    timeval timeout{ 0, 25000 };
    const int result =
        select(static_cast<int>(socket + 1), write ? nullptr : &ready, write ? &ready : nullptr, nullptr, &timeout);
    return result < 0 && Pending() ? 0 : result;
}
Socket Connect(const IPaddress& address, const std::atomic<bool>& enabled) {
    Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID)
        return INVALID;
    if (!Nonblocking(socket)) {
        Close(socket);
        return INVALID;
    }
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = address.host;
    target.sin_port = address.port;
    if (::connect(socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == 0)
        return socket;
    if (!Pending()) {
        Close(socket);
        return INVALID;
    }
    const auto deadline = SDL_GetTicks64() + 3000;
    while (enabled && SDL_GetTicks64() < deadline) {
        const int ready = Poll(socket, true);
        if (ready < 0)
            break;
        if (ready == 0)
            continue;
        int error = 0;
#ifdef _WIN32
        int size = sizeof(error);
#else
        socklen_t size = sizeof(error);
#endif
        if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) == 0 && error == 0)
            return socket;
        break;
    }
    Close(socket);
    return INVALID;
}
} // namespace

Network::~Network() {
    Disable();
}

bool Network::Enable(const char* host, uint16_t port) {
    if (isEnabled) {
        return true;
    }
    Disable(); // Join an earlier failed connection worker before replacing its thread.
    if (host == nullptr || *host == '\0' || port == 0 || SDLNet_Init() < 0) {
        return false;
    }
    initialized = true;
    if (SDLNet_ResolveHost(&networkAddress, host, port) < 0) {
        SDLNet_Quit();
        initialized = false;
        return false;
    }
    ClearQueues();
    isEnabled = true;
    try {
        receiveThread = std::thread(&Network::ReceiveFromServer, this);
    } catch (...) {
        isEnabled = false;
        SDLNet_Quit();
        initialized = false;
        return false;
    }
    return true;
}

void Network::Disable() {
    isEnabled = false;
    if (receiveThread.joinable()) {
        receiveThread.join();
    }
    ClearQueues();
    if (initialized) {
        SDLNet_Quit();
        initialized = false;
    }
}

bool Network::QueueOutgoingPacket(nlohmann::json packet) {
    if (!isConnected || !isEnabled) {
        return false;
    }
    std::string wire;
    try {
        wire = packet.dump();
    } catch (const std::exception&) { return false; }
    wire.push_back('\0');
    std::lock_guard lock(outgoingPacketQueueMutex);
    if (wire.size() > MAX_PACKET_BYTES || outgoingPacketQueue.size() >= MAX_QUEUE_PACKETS ||
        outgoingBytes + wire.size() > MAX_QUEUE_BYTES || !isConnected) {
        return false;
    }
    outgoingBytes += wire.size();
    outgoingPacketQueue.push(std::move(wire));
    return true;
}

void Network::SwapIncomingPacketQueue(std::queue<nlohmann::json>& emptyQueue) {
    if (!emptyQueue.empty()) {
        return;
    }
    std::lock_guard lock(incomingPacketQueueMutex);
    incomingPacketQueue.swap(emptyQueue);
    incomingBytes = 0;
}

void Network::ClearQueues() {
    {
        std::lock_guard lock(incomingPacketQueueMutex);
        incomingPacketQueue = {};
        incomingBytes = 0;
    }
    {
        std::lock_guard lock(outgoingPacketQueueMutex);
        outgoingPacketQueue = {};
        outgoingBytes = 0;
    }
}

bool Network::HandleCompletePacket(const std::string& payload) {
    auto packet = nlohmann::json::parse(payload, nullptr, false);
    if (!packet.is_object() || !packet.contains("type") || !packet["type"].is_string()) {
        return false;
    }
    std::lock_guard lock(incomingPacketQueueMutex);
    if (incomingPacketQueue.size() >= MAX_QUEUE_PACKETS || incomingBytes + payload.size() > MAX_QUEUE_BYTES) {
        return false;
    }
    incomingBytes += payload.size();
    incomingPacketQueue.push(std::move(packet));
    return true;
}

bool Network::ProcessOutgoingPackets() {
    std::queue<std::string> packets;
    {
        std::lock_guard lock(outgoingPacketQueueMutex);
        packets.swap(outgoingPacketQueue);
        outgoingBytes = 0;
    }
    while (!packets.empty() && isEnabled) {
        const std::string& packet = packets.front();
        size_t offset = 0;
        while (offset < packet.size() && isEnabled) {
            const Socket socket = static_cast<Socket>(networkSocket);
            const int ready = Poll(socket, true);
            if (ready < 0)
                return false;
            if (ready == 0)
                continue;
#ifdef _WIN32
            constexpr int flags = 0;
#elif defined(MSG_NOSIGNAL)
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const int sent =
                static_cast<int>(send(socket, packet.data() + offset, static_cast<int>(packet.size() - offset), flags));
            if (sent <= 0) {
                if (sent < 0 && Pending())
                    continue;
                return false;
            }
            offset += static_cast<size_t>(sent);
        }
        packets.pop();
    }
    return true;
}

void Network::ReceiveFromServer() {
    uint32_t retryDelay = 1000;
    while (isEnabled) {
        networkSocket = static_cast<intptr_t>(Connect(networkAddress, isEnabled));
        if (static_cast<Socket>(networkSocket) == INVALID) {
            uint32_t remaining = retryDelay;
            retryDelay = std::min(retryDelay * 2, 30000u);
            while (isEnabled && remaining > 0) {
                const uint32_t slice = std::min(remaining, 25u);
                SDL_Delay(slice);
                remaining -= slice;
            }
            continue;
        }
#if !defined(_WIN32) && defined(SO_NOSIGPIPE)
        int noSignal = 1;
        setsockopt(static_cast<Socket>(networkSocket), SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
        receivedData.clear();
        ClearQueues();
        ++connectionGeneration;
        isConnected = true;
        retryDelay = 1000;
        while (isEnabled) {
            try {
                const int ready = Poll(static_cast<Socket>(networkSocket), false);
                if (ready < 0) {
                    break;
                }
                if (ready > 0) {
                    char bytes[4096];
                    const int size =
                        static_cast<int>(recv(static_cast<Socket>(networkSocket), bytes, sizeof(bytes), 0));
                    if (size <= 0) {
                        if (size < 0 && Pending())
                            continue;
                        break;
                    }
                    receivedData.append(bytes, static_cast<size_t>(size));
                    bool valid = true;
                    size_t delimiter;
                    while ((delimiter = receivedData.find('\0')) != std::string::npos) {
                        if (delimiter > MAX_PACKET_BYTES || !HandleCompletePacket(receivedData.substr(0, delimiter))) {
                            valid = false;
                            break;
                        }
                        receivedData.erase(0, delimiter + 1);
                    }
                    if (!valid || receivedData.size() > MAX_PACKET_BYTES) {
                        break;
                    }
                }
                if (!ProcessOutgoingPackets()) {
                    break;
                }
            } catch (const std::exception&) {
                // A malformed/oversized frame or allocation failure cannot escape the worker.
                break;
            }
        }
        isConnected = false;
        ++connectionGeneration;
        Close(static_cast<Socket>(networkSocket));
        networkSocket = -1;
        receivedData.clear();
        ClearQueues();
    }
    isConnected = false;
}
#endif
