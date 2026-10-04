#pragma once
#ifdef ENABLE_ANCHOR
#include "color.h"
#include <cstdint>

namespace AnchorPeers {
struct Session {
    uint64_t generation = 0;
    uint64_t ownerGeneration = 0;
    uint32_t clientId = 0;
    bool connected = false;
    bool active = false;
    bool paired = false;
    Color_RGBA8 color{ 100, 255, 100, 255 };
    char room[65] = {};
    char team[65] = {};
};
using ReadSession = bool (*)(Session*);
using Send = bool (*)(const char*);
using Publish = bool (*)(const char*);
// Game-thread callbacks borrow wire strings only for the call. The embedding owner keeps its sole socket.
void SetTransport(ReadSession read, Send send, Publish publish);
void Init();
void Receive(const char* wire);
void Suspend();
void Resume();
void Shutdown();
} // namespace AnchorPeers
#endif
