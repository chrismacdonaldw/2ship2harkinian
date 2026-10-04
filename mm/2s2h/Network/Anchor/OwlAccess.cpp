#include "OwlAccess.h"
#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "variables.h"
}

namespace OwlAccess {
namespace {
EarnedCallback earnedCallback = nullptr;
bool hooksRegistered = false;
} // namespace

bool Capture(State& state) {
    if (!AnchorProgress::LocalReady()) {
        return false;
    }
    state = { true, uint16_t(gSaveContext.save.saveInfo.playerData.owlActivationFlags & MASK) };
    return true;
}

AnchorProgress::ApplyResult Apply(const State& state) {
    if (!state.known) {
        return AnchorProgress::ApplyResult::Unknown;
    }
    if ((state.mask & ~MASK) != 0) {
        return AnchorProgress::ApplyResult::Rejected;
    }
    if (!AnchorProgress::Ready()) {
        return AnchorProgress::ApplyResult::NotReady;
    }
    // Remote earned access must not select a destination or trigger the statue's randomized check.
    gSaveContext.save.saveInfo.playerData.owlActivationFlags |= state.mask;
    return AnchorProgress::ApplyResult::Applied;
}

void Register(EarnedCallback callback) {
    earnedCallback = callback;
    if (hooksRegistered || callback == nullptr) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>([](FlagType type, u32 flag) {
        if (type == FLAG_OWL_ACTIVATION && flag < 10 && earnedCallback != nullptr && AnchorProgress::LocalReady()) {
            earnedCallback(uint16_t(1u << flag));
        }
    });
    hooksRegistered = true;
}
} // namespace OwlAccess
