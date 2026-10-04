#ifndef ANCHOR_OWL_ACCESS_H
#define ANCHOR_OWL_ACCESS_H

#include "PermanentProgress.h"

namespace OwlAccess {
constexpr uint16_t MASK = 0x3FF;
struct State {
    bool known = false;
    uint16_t mask = 0;
};
using EarnedCallback = void (*)(uint16_t mask);

// Game-thread access facts only; destinations and shuffled statue rewards remain player-local.
bool Capture(State& state);
AnchorProgress::ApplyResult Apply(const State& state);
void Register(EarnedCallback callback);
} // namespace OwlAccess

#endif
