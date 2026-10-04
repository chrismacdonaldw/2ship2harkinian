#include "Network.h"
#ifdef ENABLE_ANCHOR
#include <algorithm>

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
            const int sent =
                SDLNet_TCP_Send(networkSocket, packet.data() + offset, static_cast<int>(packet.size() - offset));
            if (sent <= 0) {
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
        networkSocket = SDLNet_TCP_Open(&networkAddress);
        if (networkSocket == nullptr) {
            uint32_t remaining = retryDelay;
            retryDelay = std::min(retryDelay * 2, 30000u);
            while (isEnabled && remaining > 0) {
                const uint32_t slice = std::min(remaining, 25u);
                SDL_Delay(slice);
                remaining -= slice;
            }
            continue;
        }
        SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
        if (socketSet == nullptr || SDLNet_TCP_AddSocket(socketSet, networkSocket) < 0) {
            if (socketSet != nullptr) {
                SDLNet_FreeSocketSet(socketSet);
            }
            SDLNet_TCP_Close(networkSocket);
            networkSocket = nullptr;
            isEnabled = false;
            break;
        }
        receivedData.clear();
        ClearQueues();
        ++connectionGeneration;
        isConnected = true;
        retryDelay = 1000;
        while (isEnabled) {
            try {
                const int ready = SDLNet_CheckSockets(socketSet, 25);
                if (ready < 0) {
                    break;
                }
                if (ready > 0 && SDLNet_SocketReady(networkSocket)) {
                    char bytes[4096];
                    const int size = SDLNet_TCP_Recv(networkSocket, bytes, sizeof(bytes));
                    if (size <= 0) {
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
        SDLNet_FreeSocketSet(socketSet);
        SDLNet_TCP_Close(networkSocket);
        networkSocket = nullptr;
        receivedData.clear();
        ClearQueues();
    }
    isConnected = false;
}
#endif
