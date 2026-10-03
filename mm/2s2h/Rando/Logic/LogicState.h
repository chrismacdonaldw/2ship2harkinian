#ifndef RANDO_LOGIC_STATE_H
#define RANDO_LOGIC_STATE_H

#include "Rando/Rando.h"

extern "C" {
#include "functions.h"
#include "variables.h"
}

#include <string>
#include <vector>

#define RANDO_LOGIC_STATE 1

namespace Rando {

namespace Logic {

struct State {
    u8 items[48];
    u16 equipment;
    u32 upgrades;
    u32 questItems;
    u8 dungeonItems[10];
    s8 dungeonKeys[9];
    s8 strayFairies[10];
    s16 healthCapacity;
    u8 isMagicAcquired;
    u8 isDoubleMagicAcquired;
    u8 doubleDefense;
    u16 owlActivationFlags;
    u8 playerForm;
    u32 skullTokenCount;
    u32 stolenItems;
    u8 weekEventReg[100];
    u16 randoInf[(RANDO_INF_MAX + 15) / 16];
    u8 events[RE_MAX];
    s8 foundDungeonKeys[9];
    u16 foundTriforcePieces;
    u8 sariaHintsAvailable;
    u32 options[RO_MAX];

    const RandoSaveCheck* checks = nullptr;
    uint64_t regionTime = 0;
};

State FromSave(const SaveContext& saveContext);
void RefreshFromSave(State& state, const SaveContext& saveContext);
State NewFileState(const RandoSaveInfo& rando, const std::vector<RandoItemId>& startingItems);

void Apply(State& state, RandoItemId randoItemId);
void Remove(State& state, RandoItemId randoItemId);
bool IsObtainable(const State& state, RandoItemId randoItemId, bool checkObtained = false);
RandoItemId Resolve(const State& state, RandoItemId randoItemId, bool checkObtained = false);

std::string Diff(const State& a, const State& b);

extern thread_local State* tCurrentState;
State& LiveState();
inline State& Cur() {
    State* state = tCurrentState;
    return state != nullptr ? *state : LiveState();
}

class ScopedState {
  public:
    explicit ScopedState(State& state) : mPrevious(tCurrentState) {
        tCurrentState = &state;
    }
    ~ScopedState() {
        tCurrentState = mPrevious;
    }
    ScopedState(const ScopedState&) = delete;
    ScopedState& operator=(const ScopedState&) = delete;

  private:
    State* mPrevious;
};

inline s32 GetRandoInf(const State& state, s32 flag) {
    return state.randoInf[(flag) >> 4] & (1 << ((flag) & 0xF));
}

inline s32 HasItemInBottle(const State& state, u8 item) {
    for (s32 slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        if (state.items[slot] == item) {
            return true;
        }
    }
    return false;
}

inline s32 HasEmptyBottle(const State& state) {
    return HasItemInBottle(state, ITEM_BOTTLE);
}

inline s16 SkullTokenCount(const State& state, s16 sceneIndex) {
    if (sceneIndex == SCENE_KINSTA1) {
        return (state.skullTokenCount & 0xFFFF0000) >> 0x10;
    } else {
        return state.skullTokenCount & 0xFFFF;
    }
}

}

}

#define LS_STATE (::Rando::Logic::Cur())
#define LS_GET_PLAYER_FORM (LS_STATE.playerForm)
#define LS_INV_CONTENT(item) (LS_STATE.items[SLOT(item)])
#define LS_HEALTH_CAPACITY (LS_STATE.healthCapacity)
#define LS_IS_MAGIC_ACQUIRED (LS_STATE.isMagicAcquired)
#define LS_OWL_ACTIVATION_FLAGS (LS_STATE.owlActivationFlags)
#define LS_STRAY_FAIRIES (LS_STATE.strayFairies)
#define LS_FOUND_DUNGEON_KEYS (LS_STATE.foundDungeonKeys)
#define LS_GET_CUR_EQUIP_VALUE(equip) ((LS_STATE.equipment & gEquipMasks[equip]) >> gEquipShifts[equip])
#define LS_CUR_UPG_VALUE(upg) ((LS_STATE.upgrades & gUpgradeMasks[upg]) >> gUpgradeShifts[upg])
#define LS_GET_CUR_UPG_VALUE(upg) LS_CUR_UPG_VALUE(upg)
#define LS_CHECK_QUEST_ITEM(item) (LS_STATE.questItems & gBitFlags[item])
#define LS_CHECK_WEEKEVENTREG(flag) (LS_STATE.weekEventReg[(flag) >> 8] & ((flag) & 0xFF))
#define LS_CHECK_DUNGEON_ITEM(item, dungeonSceneIndex) (LS_STATE.dungeonItems[dungeonSceneIndex] & gBitFlags[item])
#define LS_Inventory_HasItemInBottle(item) (::Rando::Logic::HasItemInBottle(LS_STATE, item))
#define LS_Inventory_GetSkullTokenCount(sceneIndex) (::Rando::Logic::SkullTokenCount(LS_STATE, sceneIndex))
#define LS_Flags_GetRandoInf(flag) (::Rando::Logic::GetRandoInf(LS_STATE, flag))
#define LS_RANDO_EVENTS (LS_STATE.events)
#define LS_RANDO_SAVE_OPTIONS (LS_STATE.options)
#define LS_RANDO_SAVE_CHECKS (LS_STATE.checks)
#define LS_REGION_TIME (LS_STATE.regionTime)

#endif
