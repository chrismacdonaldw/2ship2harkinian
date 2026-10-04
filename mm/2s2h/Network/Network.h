#ifndef NETWORK_H
#define NETWORK_H
#ifdef ENABLE_ANCHOR

#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <SDL2/SDL_net.h>
#include <nlohmann/json.hpp>

class Network {
  public:
    virtual ~Network();
    // Called on the game thread; hostname lookup uses the system resolver synchronously.
    bool Enable(const char* host, uint16_t port);
    void Disable();
    bool QueueOutgoingPacket(nlohmann::json packet);
    void SwapIncomingPacketQueue(std::queue<nlohmann::json>& emptyQueue);

    std::atomic<bool> isEnabled{ false };
    std::atomic<bool> isConnected{ false };
    std::atomic<uint64_t> connectionGeneration{ 0 };

  private:
    static constexpr size_t MAX_PACKET_BYTES = 1024 * 1024;
    static constexpr size_t MAX_QUEUE_BYTES = 4 * MAX_PACKET_BYTES;
    static constexpr size_t MAX_QUEUE_PACKETS = 256;
    IPaddress networkAddress{};
    intptr_t networkSocket = -1;
    std::thread receiveThread;
    bool initialized = false; // Enable/Disable are called only by the game thread.
    std::string receivedData;
    std::mutex incomingPacketQueueMutex;
    std::queue<nlohmann::json> incomingPacketQueue;
    size_t incomingBytes = 0;
    std::mutex outgoingPacketQueueMutex;
    std::queue<std::string> outgoingPacketQueue;
    size_t outgoingBytes = 0;

    void ReceiveFromServer();
    bool HandleCompletePacket(const std::string& payload);
    bool ProcessOutgoingPackets();
    void ClearQueues();
};

#endif
#endif
