// Compile with ENABLE_ANCHOR, linking c-ares, SDL2_net and SDL2.
// Include the actual transport once so its private resolver can use a local stalled DNS fixture.
#include "../mm/2s2h/Network/Network.cpp"
#ifdef main
#undef main
#endif
#include <chrono>
#include <iostream>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <class Predicate> static bool Wait(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;
        SDL_Delay(5);
    }
    return false;
}

int main() {
    Require(SDLNet_Init() == 0, "server SDLNet_Init");
    Require(ares_library_init(ARES_LIB_INIT_ALL) == ARES_SUCCESS, "resolver init");
    std::atomic<bool> enabled{ true };
    IPaddress resolved{};
    Require(Resolve("localhost", resolved, enabled), "normal hostname failed");
    Socket dns = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    Require(dns != INVALID && Nonblocking(dns), "DNS fixture socket");
    sockaddr_in dnsAddress{};
    dnsAddress.sin_family = AF_INET;
    dnsAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Require(bind(dns, reinterpret_cast<sockaddr*>(&dnsAddress), sizeof(dnsAddress)) == 0, "DNS fixture bind");
#ifdef _WIN32
    int addressSize = sizeof(dnsAddress);
#else
    socklen_t addressSize = sizeof(dnsAddress);
#endif
    Require(getsockname(dns, reinterpret_cast<sockaddr*>(&dnsAddress), &addressSize) == 0, "DNS fixture address");
    const std::string resolver = "127.0.0.1:" + std::to_string(ntohs(dnsAddress.sin_port));
    bool result = true;
    std::thread query([&] { result = Resolve("never-answers.invalid", resolved, enabled, resolver.c_str()); });
    Require(Wait([&] { return Poll(dns, false) > 0; }), "resolver query not sent");
    // Leave the DNS request unanswered, then cancel the actual resolver loop.
    const auto cancel = std::chrono::steady_clock::now();
    enabled = false;
    query.join();
    Require(!result && std::chrono::steady_clock::now() - cancel < std::chrono::seconds(1),
            "stalled DNS prevented cancellation");
    Close(dns);
    ares_library_cleanup();
    TCPsocket listener = nullptr;
    uint16_t port = 62000;
    IPaddress address{};
    for (; port < 62100 && listener == nullptr; ++port) {
        Require(SDLNet_ResolveHost(&address, nullptr, port) == 0, "server resolve");
        listener = SDLNet_TCP_Open(&address);
    }
    Require(listener != nullptr, "no disposable listening port");
    --port;
    Network client;
    Require(!client.Enable(nullptr, port), "invalid host accepted");
    Require(client.Enable("127.0.0.1", port), "enable failed");
    TCPsocket peer = nullptr;
    Require(Wait([&] {
                peer = SDLNet_TCP_Accept(listener);
                return peer != nullptr;
            }),
            "connect timeout");
    Require(Wait([&] { return client.isConnected.load(); }), "connected state timeout");
    const auto firstGeneration = client.connectionGeneration.load();
    std::queue<nlohmann::json> packets;
    const std::string fragment = "{\"type\":\"ONE\",\"value\":";
    Require(SDLNet_TCP_Send(peer, fragment.data(), static_cast<int>(fragment.size())) ==
                static_cast<int>(fragment.size()),
            "fragment send");
    SDL_Delay(40);
    client.SwapIncomingPacketQueue(packets);
    Require(packets.empty(), "partial frame delivered");
    const std::string tail = std::string("1}") + '\0' + "{\"type\":\"TWO\"}" + '\0';
    Require(SDLNet_TCP_Send(peer, tail.data(), static_cast<int>(tail.size())) == static_cast<int>(tail.size()),
            "tail send");
    Require(Wait([&] {
                client.SwapIncomingPacketQueue(packets);
                return !packets.empty();
            }),
            "complete frame timeout");
    Require(packets.front()["type"] == "ONE", "frame order");
    packets.pop();
    if (packets.empty())
        Require(Wait([&] {
                    client.SwapIncomingPacketQueue(packets);
                    return !packets.empty();
                }),
                "second frame timeout");
    Require(packets.front()["type"] == "TWO", "coalesced frame missing");
    packets.pop();
    Require(client.QueueOutgoingPacket({ { "type", "REPLY" } }), "outbound queue failed");
    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    Require(set && SDLNet_TCP_AddSocket(set, peer) >= 0, "server socket set");
    Require(Wait([&] { return SDLNet_CheckSockets(set, 5) > 0 && SDLNet_SocketReady(peer); }), "outbound timeout");
    char bytes[1024]{};
    const int count = SDLNet_TCP_Recv(peer, bytes, sizeof(bytes));
    Require(count > 1 && bytes[count - 1] == '\0' && nlohmann::json::parse(bytes)["type"] == "REPLY",
            "outbound framing");
    SDLNet_FreeSocketSet(set);
    SDLNet_TCP_Close(peer);
    peer = nullptr;
    Require(Wait([&] { return client.connectionGeneration.load() > firstGeneration; }),
            "disconnect generation missing");
    Require(Wait([&] {
                peer = SDLNet_TCP_Accept(listener);
                return peer != nullptr;
            }),
            "reconnect timeout");
    Require(Wait([&] { return client.isConnected.load(); }), "reconnect connected state");
    const std::string invalid = std::string("not-json") + '\0';
    const auto secondGeneration = client.connectionGeneration.load();
    Require(SDLNet_TCP_Send(peer, invalid.data(), static_cast<int>(invalid.size())) > 0, "invalid send");
    Require(Wait([&] { return client.connectionGeneration.load() > secondGeneration; }),
            "invalid frame did not reset connection");
    SDLNet_TCP_Close(peer);
    client.Disable();
    client.Disable();
    Require(!client.isConnected && !client.isEnabled, "disable state");
    Require(!client.QueueOutgoingPacket({ { "type", "LATE" } }), "disabled queue accepted");
    Require(client.Enable("127.0.0.1", port), "reenable failed");
    peer = nullptr;
    Require(Wait([&] {
                peer = SDLNet_TCP_Accept(listener);
                return peer != nullptr;
            }),
            "stalled peer connect");
    Require(Wait([&] { return client.isConnected.load(); }), "stalled peer state");
    // The peer intentionally never reads. Fill beyond normal TCP buffers, then stop the sender.
    const nlohmann::json large{ { "type", "PRESSURE" }, { "payload", std::string(900000, 'x') } };
    for (int i = 0; i < 24; ++i) {
        client.QueueOutgoingPacket(large);
        SDL_Delay(10);
    }
    const auto stop = std::chrono::steady_clock::now();
    client.Disable();
    Require(std::chrono::steady_clock::now() - stop < std::chrono::seconds(1), "stalled send prevented shutdown");
    SDLNet_TCP_Close(peer);
    SDLNet_TCP_Close(listener);
    SDLNet_Quit();
    std::cout << "Native transport framing/reconnect/shutdown passed\n";
}
