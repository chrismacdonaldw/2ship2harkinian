#include "PermanentProgress.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/StaticData/StaticData.h"

extern "C" {
#include "variables.h"
#include "functions.h"
}

namespace AnchorProgress {
namespace {
EditCallback editCallback = nullptr;
bool hooksRegistered = false;
uint32_t remoteDepth = 0;

struct RemoteScope {
    RemoteScope() {
        ++remoteDepth;
    }
    ~RemoteScope() {
        --remoteDepth;
    }
};

void BuildMasks(State& masks) {
    for (int16_t scene = 0; scene < SCENE_MAX; ++scene) {
        if (CanonicalScene(scene) == scene) {
            for (uint8_t bank = 0; bank < 2; ++bank) {
                masks.switches[scene][bank] = Sram_GetPersistentCycleSwitchMask(scene, bank);
            }
        }
    }
    // Check flags can enable rewards and are cleared by randomizer cycle saving even when unshuffled.
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        if (check.flagType != FLAG_CYCL_SCENE_SWITCH || check.flag < 0 || check.flag >= 64) {
            continue;
        }
        int16_t scene = CanonicalScene(check.sceneId);
        if (scene >= 0 && scene < SCENE_MAX) {
            masks.switches[scene][check.flag / 32] &= ~(uint32_t(1) << (check.flag % 32));
        }
    }
}

bool LoadedOwner() {
    return gPlayState != nullptr && gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.fileNum != 0xFF &&
           gPlayState->state.running && GET_PLAYER(gPlayState) != nullptr && CanonicalScene(gPlayState->sceneId) >= 0 &&
           !Rando::StaticData::Checks.empty();
}

bool CanCapture() {
    return LoadedOwner() && gPlayState->transitionTrigger == TRANS_TRIGGER_OFF &&
           gPlayState->transitionMode == TRANS_MODE_OFF;
}

void Emit(int16_t scene, FlagType type, uint32_t flag, bool set) {
    if (editCallback == nullptr || remoteDepth != 0 || type != FLAG_CYCL_SCENE_SWITCH || flag >= 64 || !LoadedOwner()) {
        return;
    }
    scene = CanonicalScene(scene);
    if (scene == CanonicalScene(gPlayState->sceneId) &&
        (EligibleMask(scene, flag / 32) & (uint32_t(1) << (flag % 32))) != 0) {
        editCallback(Edit{ scene, uint8_t(flag), set });
    }
}

void ApplyBank(int16_t scene, uint8_t bank, uint32_t value, uint32_t mask) {
    auto& cycle = gSaveContext.cycleSceneFlags[scene];
    auto& saved = gSaveContext.save.saveInfo.permanentSceneFlags[scene];
    uint32_t& cycleBank = bank == 0 ? cycle.switch0 : cycle.switch1;
    uint32_t& savedBank = bank == 0 ? saved.switch0 : saved.switch1;
    cycleBank = (cycleBank & ~mask) | (value & mask);
    savedBank = (savedBank & ~mask) | (value & mask);
    if (CanonicalScene(gPlayState->sceneId) != scene) {
        return;
    }
    uint32_t changed = (gPlayState->actorCtx.sceneFlags.switches[bank] ^ value) & mask;
    for (uint8_t bit = 0; bit < 32; ++bit) {
        if ((changed & (uint32_t(1) << bit)) != 0) {
            if ((value & (uint32_t(1) << bit)) != 0) {
                Flags_SetSwitch(gPlayState, bank * 32 + bit);
            } else {
                Flags_UnsetSwitch(gPlayState, bank * 32 + bit);
            }
        }
    }
}
} // namespace

int16_t CanonicalScene(int16_t scene) {
    return scene >= 0 && scene < SCENE_MAX ? Play_GetOriginalSceneId(scene) : -1;
}

uint32_t EligibleMask(int16_t scene, uint8_t bank) {
    scene = CanonicalScene(scene);
    if (scene < 0 || bank >= 2 || Rando::StaticData::Checks.empty()) {
        return 0;
    }
    State masks;
    BuildMasks(masks);
    return masks.switches[scene][bank];
}

bool LocalReady() {
    return LoadedOwner();
}

bool Ready() {
    return CanCapture() && gPlayState->sramCtx.status == 0;
}

bool Capture(State& state) {
    if (!CanCapture()) {
        return false;
    }
    State masks;
    BuildMasks(masks);
    State captured;
    int16_t current = CanonicalScene(gPlayState->sceneId);
    for (int16_t scene = 0; scene < SCENE_MAX; ++scene) {
        const auto& cycle = gSaveContext.cycleSceneFlags[scene];
        for (uint8_t bank = 0; bank < 2; ++bank) {
            // ActorContext is newer than saved banks after a local SET or UNSET.
            uint32_t value = scene == current ? gPlayState->actorCtx.sceneFlags.switches[bank]
                                              : (bank == 0 ? cycle.switch0 : cycle.switch1);
            captured.switches[scene][bank] = value & masks.switches[scene][bank];
        }
    }
    captured.known = true;
    state = captured;
    return true;
}

ApplyResult Apply(const State& state) {
    if (!state.known) {
        return ApplyResult::Unknown;
    }
    if (!Ready()) {
        return ApplyResult::NotReady;
    }
    State masks;
    BuildMasks(masks);
    for (int16_t scene = 0; scene < SCENE_MAX; ++scene) {
        for (uint8_t bank = 0; bank < 2; ++bank) {
            if ((state.switches[scene][bank] & ~masks.switches[scene][bank]) != 0) {
                return ApplyResult::Rejected;
            }
        }
    }
    RemoteScope scope;
    for (int16_t scene = 0; scene < SCENE_MAX; ++scene) {
        for (uint8_t bank = 0; bank < 2; ++bank) {
            ApplyBank(scene, bank, state.switches[scene][bank], masks.switches[scene][bank]);
        }
    }
    return ApplyResult::Applied;
}

ApplyResult Apply(const Edit& edit) {
    if (!Ready()) {
        return ApplyResult::NotReady;
    }
    int16_t scene = CanonicalScene(edit.scene);
    if (scene < 0 || edit.flag >= 64) {
        return ApplyResult::Rejected;
    }
    uint8_t bank = edit.flag / 32;
    uint32_t mask = uint32_t(1) << (edit.flag % 32);
    if ((EligibleMask(scene, bank) & mask) == 0) {
        return ApplyResult::Rejected;
    }
    RemoteScope scope;
    ApplyBank(scene, bank, edit.set ? mask : 0, mask);
    return ApplyResult::Applied;
}

bool IsApplyingRemote() {
    return remoteDepth != 0;
}

void Register(EditCallback callback) {
    editCallback = callback;
    if (hooksRegistered || callback == nullptr) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagSet>(
        [](s16 scene, FlagType type, u32 flag) { Emit(scene, type, flag, true); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagUnset>(
        [](s16 scene, FlagType type, u32 flag) { Emit(scene, type, flag, false); });
    hooksRegistered = true;
}

} // namespace AnchorProgress
