#ifndef ANCHOR_PERMANENT_PROGRESS_H
#define ANCHOR_PERMANENT_PROGRESS_H

#include <cstdint>
extern "C" {
#include "z64scene.h"
}

namespace AnchorProgress {

struct State {
    bool known = false;
    uint32_t switches[SCENE_MAX][2] = {};
};

struct Edit {
    int16_t scene;
    uint8_t flag;
    bool set;
};

enum class ApplyResult { Rejected, NotReady, Unknown, Applied };
using EditCallback = void (*)(const Edit&);

// All entrypoints and callback binding belong to the game thread.
int16_t CanonicalScene(int16_t scene);
uint32_t EligibleMask(int16_t scene, uint8_t bank);
bool Ready();
bool Capture(State& state);
ApplyResult Apply(const State& state);
ApplyResult Apply(const Edit& edit);
bool IsApplyingRemote();
void Register(EditCallback callback);

} // namespace AnchorProgress

#endif
