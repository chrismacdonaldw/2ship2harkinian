#pragma once

#include "LogicState.h"
#include "Rando/MiscBehavior/ClockShuffle.h"

namespace Rando {

namespace Logic {

#define S_INV_CONTENT(s, item) ((s).items[SLOT(item)])

inline u32 CurUpgValue(const State& s, s16 upgrade) {
    return (s.upgrades & gUpgradeMasks[upgrade]) >> gUpgradeShifts[upgrade];
}

inline void ChangeUpgrade(State& s, s16 upgrade, u32 value) {
    u32 upgrades = s.upgrades;
    upgrades &= gUpgradeNegMasks[upgrade];
    upgrades |= value << gUpgradeShifts[upgrade];
    s.upgrades = upgrades;
}

inline u16 CurEquipValue(const State& s, s32 equip) {
    return (s.equipment & gEquipMasks[equip]) >> gEquipShifts[equip];
}

inline void SetEquipValue(State& s, s32 equip, u16 value) {
    s.equipment = ((s.equipment & gEquipNegMasks[equip]) | (u16)((u16)(value) << gEquipShifts[equip]));
}

inline u32 CheckQuestItem(const State& s, s32 item) {
    return s.questItems & gBitFlags[item];
}

inline void SetQuestItem(State& s, s32 item) {
    s.questItems = s.questItems | gBitFlags[item];
}

inline void RemoveQuestItem(State& s, s32 item) {
    s.questItems = s.questItems & (-1 - gBitFlags[item]);
}

#define S_HEART_PIECE_COUNT(s) (((s).questItems & 0xF0000000) >> QUEST_HEART_PIECE_COUNT)

inline s32 CheckWeekEventReg(const State& s, s32 flag) {
    return s.weekEventReg[(flag) >> 8] & ((flag) & 0xFF);
}

inline void SetWeekEventReg(State& s, s32 flag) {
    s.weekEventReg[(flag) >> 8] = s.weekEventReg[(flag) >> 8] | ((flag) & 0xFF);
}

inline void ClearWeekEventReg(State& s, s32 flag) {
    s.weekEventReg[(flag) >> 8] = s.weekEventReg[(flag) >> 8] & (u8) ~((flag) & 0xFF);
}

inline void SetRandoInf(State& s, s32 flag) {
    s.randoInf[flag >> 4] |= (1 << (flag & 0xF));
}

inline void ClearRandoInf(State& s, s32 flag) {
    s.randoInf[flag >> 4] &= ~(1 << (flag & 0xF));
}

inline u32 CheckDungeonItem(const State& s, s32 item, s32 dungeonSceneIndex) {
    return s.dungeonItems[dungeonSceneIndex] & gBitFlags[item];
}

inline void SetDungeonItem(State& s, s32 item, s32 dungeonSceneIndex) {
    s.dungeonItems[dungeonSceneIndex] |= (u8)gBitFlags[item];
}

inline void RemoveDungeonItem(State& s, s32 item, s32 dungeonSceneIndex) {
    s.dungeonItems[dungeonSceneIndex] &= ~(u8)gBitFlags[item];
}

inline u32 StolenItem1(const State& s) {
    return (s.stolenItems & 0xFF000000) >> 0x18;
}

inline u32 StolenItem2(const State& s) {
    return (s.stolenItems & 0x00FF0000) >> 0x10;
}

inline void SetStolenItem1(State& s, u32 itemId) {
    s.stolenItems = (s.stolenItems & ~0xFF000000) | ((itemId & 0xFF) << 0x18);
}

inline void SetStolenItem2(State& s, u32 itemId) {
    s.stolenItems = (s.stolenItems & ~0x00FF0000) | ((itemId & 0xFF) << 0x10);
}

inline bool CanOwlWarp(const State& s, s32 owlId) {
    return (s.owlActivationFlags >> owlId) & 1;
}

inline void ActivateOwl(State& s, u8 owlWarpId) {
    s.owlActivationFlags = s.owlActivationFlags | (u16)gBitFlags[(owlWarpId)];
}

inline void ClearOwl(State& s, s32 owlId) {
    s.owlActivationFlags &= ~(1 << owlId);
}

inline void IncrementSkullTokenCount(State& s, s16 sceneIndex) {
    if (sceneIndex == SCENE_KINSTA1) {
        s.skullTokenCount =
            ((u16)(((s.skullTokenCount & 0xFFFF0000) >> 0x10) + 1) << 0x10) | (s.skullTokenCount & 0xFFFF);
    } else {
        s.skullTokenCount = (((u16)s.skullTokenCount + 1) & 0xFFFF) | (s.skullTokenCount & 0xFFFF0000);
    }
}

inline void DeleteItem(State& s, s16 slot) {
    s.items[slot] = ITEM_NONE;
}

inline bool OwnsClockHalfDayIn(const State& s, int halfDayIndex) {
    if (halfDayIndex < 0 || halfDayIndex >= 6) {
        return false;
    }
    return GetRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 + halfDayIndex);
}

inline u8 AllOwnedHalfDaysMask(const State& s) {
    u8 ownedMask = 0;
    for (int i = 0; i < Rando::ClockItems::HALF_COUNT; ++i) {
        if (OwnsClockHalfDayIn(s, i)) {
            ownedMask |= (1 << i);
        }
    }
    return ownedMask;
}

inline int FindOwnedHalfDayIn(const State& s, bool fromEnd) {
    if (fromEnd) {
        for (int i = Rando::ClockItems::HALF_COUNT - 1; i >= 0; --i) {
            if (OwnsClockHalfDayIn(s, i)) {
                return i;
            }
        }
    } else {
        for (int i = 0; i < Rando::ClockItems::HALF_COUNT; ++i) {
            if (OwnsClockHalfDayIn(s, i)) {
                return i;
            }
        }
    }
    return -1;
}

#define LS_SOUL_CASES                 \
    case RI_SOUL_BOSS_GOHT:           \
    case RI_SOUL_BOSS_GYORG:          \
    case RI_SOUL_BOSS_MAJORA:         \
    case RI_SOUL_BOSS_ODOLWA:         \
    case RI_SOUL_BOSS_TWINMOLD:       \
    case RI_SOUL_ENEMY_ALIEN:         \
    case RI_SOUL_ENEMY_ARMOS:         \
    case RI_SOUL_ENEMY_BAD_BAT:       \
    case RI_SOUL_ENEMY_BEAMOS:        \
    case RI_SOUL_ENEMY_BOE:           \
    case RI_SOUL_ENEMY_BUBBLE:        \
    case RI_SOUL_ENEMY_CAPTAIN_KEETA: \
    case RI_SOUL_ENEMY_CHUCHU:        \
    case RI_SOUL_ENEMY_DEATH_ARMOS:   \
    case RI_SOUL_ENEMY_DEEP_PYTHON:   \
    case RI_SOUL_ENEMY_DEKU_BABA:     \
    case RI_SOUL_ENEMY_DEXIHAND:      \
    case RI_SOUL_ENEMY_DINOLFOS:      \
    case RI_SOUL_ENEMY_DODONGO:       \
    case RI_SOUL_ENEMY_DRAGONFLY:     \
    case RI_SOUL_ENEMY_EENO:          \
    case RI_SOUL_ENEMY_EYEGORE:       \
    case RI_SOUL_ENEMY_FREEZARD:      \
    case RI_SOUL_ENEMY_GARO:          \
    case RI_SOUL_ENEMY_GEKKO:         \
    case RI_SOUL_ENEMY_GIANT_BEE:     \
    case RI_SOUL_ENEMY_GOMESS:        \
    case RI_SOUL_ENEMY_GUAY:          \
    case RI_SOUL_ENEMY_HIPLOOP:       \
    case RI_SOUL_ENEMY_IGOS_DU_IKANA: \
    case RI_SOUL_ENEMY_IRON_KNUCKLE:  \
    case RI_SOUL_ENEMY_KEESE:         \
    case RI_SOUL_ENEMY_LEEVER:        \
    case RI_SOUL_ENEMY_LIKE_LIKE:     \
    case RI_SOUL_ENEMY_MAD_SCRUB:     \
    case RI_SOUL_ENEMY_NEJIRON:       \
    case RI_SOUL_ENEMY_OCTOROK:       \
    case RI_SOUL_ENEMY_PEAHAT:        \
    case RI_SOUL_ENEMY_PIRATE:        \
    case RI_SOUL_ENEMY_POE:           \
    case RI_SOUL_ENEMY_REDEAD:        \
    case RI_SOUL_ENEMY_SHELLBLADE:    \
    case RI_SOUL_ENEMY_SKULLFISH:     \
    case RI_SOUL_ENEMY_SKULLTULA:     \
    case RI_SOUL_ENEMY_SNAPPER:       \
    case RI_SOUL_ENEMY_STALCHILD:     \
    case RI_SOUL_ENEMY_TAKKURI:       \
    case RI_SOUL_ENEMY_TEKTITE:       \
    case RI_SOUL_ENEMY_WALLMASTER:    \
    case RI_SOUL_ENEMY_WART:          \
    case RI_SOUL_ENEMY_WIZROBE:       \
    case RI_SOUL_ENEMY_WOLFOS

}

}
