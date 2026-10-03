#include "LogicStateFields.h"
#include "Rando/ActorBehavior/Souls.h"

typedef struct GetItemEntry {
    u8 itemId;
    u8 field;
    s8 gid;
    u8 textId;
    u16 objectId;
} GetItemEntry;
#define GIFIELD_20 (1 << 5)
#define GIFIELD_40 (1 << 6)

extern "C" {
extern GetItemEntry sGetItemTable[GI_MAX - 1];
extern s16 sExtraItemBases[];
}

namespace Rando {

namespace Logic {

namespace {

u8 CheckVanillaObtainability(const State& s, u8 item) {
    s16 i;
    u8 slot;
    u8 bottleSlot;

    if (item == ITEM_SHIP) {
        return ITEM_NONE;
    }

    if (item >= ITEM_DEKU_STICKS_5) {
        slot = SLOT(sExtraItemBases[item - ITEM_DEKU_STICKS_5]);
    } else if (item < ARRAY_COUNT(gItemSlots)) {
        slot = SLOT(item);
    } else {
        slot = 0xFF;
    }

    if (item == ITEM_SKULL_TOKEN) {
        return ITEM_NONE;
    } else if (item == ITEM_TINGLE_MAP) {
        return ITEM_NONE;
    } else if (item == ITEM_BOMBERS_NOTEBOOK) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SHIELD_HERO) && (item <= ITEM_SHIELD_MIRROR)) {
        return ITEM_NONE;
    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP)) {
        return ITEM_NONE;
    } else if (item == ITEM_KEY_SMALL) {
        return ITEM_NONE;
    } else if ((item == ITEM_OCARINA_OF_TIME) || (item == ITEM_BOMBCHU) || (item == ITEM_HOOKSHOT) ||
               (item == ITEM_LENS_OF_TRUTH) || (item == ITEM_SWORD_GREAT_FAIRY) || (item == ITEM_PICTOGRAPH_BOX)) {
        if (S_INV_CONTENT(s, item) == ITEM_NONE) {
            return ITEM_NONE;
        }
        return S_INV_CONTENT(s, item);
    } else if ((item >= ITEM_BOMBS_5) && (item == ITEM_BOMBS_30)) {
        // Preserve the native equality check; it should be a range check.
        if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
            return ITEM_NONE;
        }
        return 0;
    } else if ((item >= ITEM_BOMBCHUS_20) && (item <= ITEM_BOMBCHUS_5)) {
        if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
            return ITEM_NONE;
        }
        return 0;
    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CurUpgValue(s, UPG_QUIVER) == 0) {
            return ITEM_NONE;
        }
        return 0;
    } else if ((item == ITEM_QUIVER_40) || (item == ITEM_QUIVER_50)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BOMB_BAG_20) || (item == ITEM_BOMB)) {
        if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
            return ITEM_NONE;
        }
        return 0;
    } else if ((item >= ITEM_DEKU_STICK_UPGRADE_20) && (item <= ITEM_DEKU_NUT_UPGRADE_40)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_BOMB_BAG_30) && (item <= ITEM_WALLET_GIANT)) {
        return ITEM_NONE;
    } else if (item == ITEM_MAGIC_BEANS) {
        return ITEM_NONE;
    } else if (item == ITEM_POWDER_KEG) {
        return ITEM_NONE;
    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        return ITEM_NONE;
    } else if (item == ITEM_HEART_CONTAINER) {
        return ITEM_NONE;
    } else if (item == ITEM_RECOVERY_HEART) {
        return ITEM_RECOVERY_HEART;
    } else if ((item == ITEM_MAGIC_JAR_SMALL) || (item == ITEM_MAGIC_JAR_BIG)) {
        if (!CheckWeekEventReg(s, WEEKEVENTREG_12_80)) {
            return ITEM_NONE;
        }
        return item;
    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_RUPEE_HUGE)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_REMAINS_ODOLWA) && (item <= ITEM_REMAINS_TWINMOLD)) {
        return ITEM_NONE;
    } else if (item == ITEM_LONGSHOT) {
        return ITEM_NONE;
    } else if (item == ITEM_BOTTLE) {
        return ITEM_NONE;
    } else if ((item == ITEM_MILK_BOTTLE) || (item == ITEM_POE) || (item == ITEM_GOLD_DUST) || (item == ITEM_CHATEAU) ||
               (item == ITEM_HYLIAN_LOACH)) {
        return ITEM_NONE;
    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_OBABA_DRINK)) || (item == ITEM_CHATEAU_2) ||
               (item == ITEM_MILK) || (item == ITEM_GOLD_DUST_2) || (item == ITEM_HYLIAN_LOACH_2) ||
               (item == ITEM_SEAHORSE_CAUGHT)) {
        bottleSlot = (item < ARRAY_COUNT(gItemSlots)) ? SLOT(item) : SLOT_BOTTLE_1;

        if ((item != ITEM_MILK_BOTTLE) && (item != ITEM_MILK_HALF)) {
            if (item == ITEM_CHATEAU_2) {
                item = ITEM_CHATEAU;
            } else if (item == ITEM_MILK) {
                item = ITEM_MILK_BOTTLE;
            } else if (item == ITEM_GOLD_DUST_2) {
                item = ITEM_GOLD_DUST;
            } else if (item == ITEM_HYLIAN_LOACH_2) {
                item = ITEM_HYLIAN_LOACH;
            } else if (item == ITEM_SEAHORSE_CAUGHT) {
                item = ITEM_SEAHORSE;
            }
            bottleSlot = SLOT(item);

            for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
                if (s.items[bottleSlot + i] == ITEM_BOTTLE) {
                    return ITEM_NONE;
                }
            }
        } else {
            for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
                if (s.items[bottleSlot + i] == ITEM_NONE) {
                    return ITEM_NONE;
                }
            }
        }
    } else if ((item >= ITEM_MOONS_TEAR) && (item <= ITEM_MASK_GIANT)) {
        return ITEM_NONE;
    }

    return slot < ARRAY_COUNT(s.items) ? s.items[slot] : ITEM_NONE;
}

}

bool IsObtainable(const State& s, RandoItemId randoItemId, bool hasObtainedCheck) {
    auto vanillaCantObtain = [&]() -> bool {
        if (Rando::StaticData::Items[randoItemId].itemId != ITEM_NONE &&
            Rando::StaticData::Items[randoItemId].getItemId != GI_NONE) {
            GetItemEntry* giEntry = &sGetItemTable[Rando::StaticData::Items[randoItemId].getItemId - 1];
            u8 obtainability = CheckVanillaObtainability(s, giEntry->itemId);
            return ((obtainability != ITEM_NONE) && (giEntry->field & GIFIELD_20)) ||
                   ((obtainability == ITEM_NONE) && (giEntry->field & GIFIELD_40));
        }
        return false;
    };

    switch (randoItemId) {
        case RI_UNKNOWN:
            return false;
        case RI_PROGRESSIVE_WALLET:
            if (hasObtainedCheck) {
                return false;
            } else if (s.options[RO_SHUFFLE_TYCOON_WALLET] == RO_GENERIC_YES) {
                if (CurUpgValue(s, UPG_WALLET) >= 3) {
                    return false;
                }
            } else if (CurUpgValue(s, UPG_WALLET) >= 2) {
                return false;
            }
            return true;
        case RI_WALLET_ADULT:
            if (CurUpgValue(s, UPG_WALLET) >= 1) {
                return false;
            }
            break;
        case RI_WALLET_GIANT:
            if (CurUpgValue(s, UPG_WALLET) >= 2) {
                return false;
            }
            break;
        case RI_WALLET_TYCOON:
            if (CurUpgValue(s, UPG_WALLET) >= 3) {
                return false;
            }
            break;
        case RI_PROGRESSIVE_SWORD:
            if (hasObtainedCheck) {
                return false;
            } else if (CurEquipValue(s, EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_GILDED ||
                       (StolenItem1(s) >= ITEM_SWORD_GILDED) || (StolenItem2(s) >= ITEM_SWORD_GILDED)) {
                return false;
            }
            return true;
        case RI_SWORD_KOKIRI:
            if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_KOKIRI ||
                (StolenItem1(s) >= ITEM_SWORD_KOKIRI) || (StolenItem2(s) >= ITEM_SWORD_KOKIRI)) {
                return false;
            }
            break;
        case RI_SWORD_RAZOR:
            if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_RAZOR || (StolenItem1(s) >= ITEM_SWORD_RAZOR) ||
                (StolenItem2(s) >= ITEM_SWORD_RAZOR)) {
                return false;
            }
            break;
        case RI_SWORD_GILDED:
            if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_GILDED ||
                (StolenItem1(s) >= ITEM_SWORD_GILDED) || (StolenItem2(s) >= ITEM_SWORD_GILDED)) {
                return false;
            }
            break;
        case RI_PROGRESSIVE_BOMB_BAG:
            if (hasObtainedCheck) {
                return false;
            } else if (CurUpgValue(s, UPG_BOMB_BAG) >= 3) {
                return false;
            }
            return true;
        case RI_BOMB_BAG_20:
            if (CurUpgValue(s, UPG_BOMB_BAG) >= 1) {
                return false;
            }
            break;
        case RI_BOMB_BAG_30:
            if (CurUpgValue(s, UPG_BOMB_BAG) >= 2) {
                return false;
            }
            break;
        case RI_BOMB_BAG_40:
            if (CurUpgValue(s, UPG_BOMB_BAG) >= 3) {
                return false;
            }
            break;
        case RI_PROGRESSIVE_BOW:
            if (hasObtainedCheck) {
                return false;
            } else if (CurUpgValue(s, UPG_QUIVER) >= 3) {
                return false;
            }
            return true;
        case RI_BOW:
            if (CurUpgValue(s, UPG_QUIVER) >= 1) {
                return false;
            }
            break;
        case RI_QUIVER_40:
            if (CurUpgValue(s, UPG_QUIVER) >= 2) {
                return false;
            }
            break;
        case RI_QUIVER_50:
            if (CurUpgValue(s, UPG_QUIVER) >= 3) {
                return false;
            }
            break;
        case RI_PROGRESSIVE_LULLABY:
            if (hasObtainedCheck) {
                return false;
            } else if (CheckQuestItem(s, QUEST_SONG_LULLABY_INTRO) && CheckQuestItem(s, QUEST_SONG_LULLABY)) {
                return false;
            }
            return true;
        case RI_PROGRESSIVE_MAGIC:
            if (hasObtainedCheck) {
                return false;
            } else if (s.isMagicAcquired && s.isDoubleMagicAcquired) {
                return false;
            }
            return true;
        case RI_DOUBLE_MAGIC:
            return !s.isDoubleMagicAcquired;
        case RI_SINGLE_MAGIC:
            return !s.isMagicAcquired;
        case RI_MAGIC_JAR_SMALL:
        case RI_MAGIC_JAR_BIG:
            return s.isMagicAcquired;
        case RI_WOODFALL_SMALL_KEY:
        case RI_SNOWHEAD_SMALL_KEY:
        case RI_GREAT_BAY_SMALL_KEY:
        case RI_STONE_TOWER_SMALL_KEY:
        case RI_WOODFALL_STRAY_FAIRY:
        case RI_SNOWHEAD_STRAY_FAIRY:
        case RI_GREAT_BAY_STRAY_FAIRY:
        case RI_STONE_TOWER_STRAY_FAIRY:
        case RI_GS_TOKEN_SWAMP:
        case RI_GS_TOKEN_OCEAN:
        case RI_TRIFORCE_PIECE:
            if (hasObtainedCheck) {
                return false;
            }
            return true;
        case RI_CLOCK_TOWN_STRAY_FAIRY:
            return !CheckWeekEventReg(s, WEEKEVENTREG_08_80);
        case RI_HEART_PIECE:
        case RI_HEART_CONTAINER:
            if (hasObtainedCheck) {
                return false;
            }
            break;
        case RI_GOLD_DUST_REFILL:
        case RI_MILK_REFILL:
        case RI_CHATEAU_ROMANI_REFILL:
        case RI_FAIRY_REFILL:
        case RI_RED_POTION_REFILL:
        case RI_BLUE_POTION_REFILL:
        case RI_GREEN_POTION_REFILL:
            if (!HasEmptyBottle(s)) {
                return false;
            }
            break;
        case RI_ARROWS_10:
        case RI_ARROWS_30:
        case RI_ARROWS_50:
            if (CurUpgValue(s, UPG_QUIVER) == 0) {
                return false;
            }
            break;
        case RI_BOMBCHU:
        case RI_BOMBCHU_5:
        case RI_BOMBCHU_10:
        case RI_BOMBS_5:
        case RI_BOMBS_10:
            if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
                return false;
            }
            break;
        case RI_SHIELD_HERO:
            if (CurEquipValue(s, EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE) {
                return false;
            }
            break;
        case RI_SHIELD_MIRROR:
            if (CurEquipValue(s, EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) {
                return false;
            }
            break;
        case RI_BOTTLE_EMPTY:
        case RI_BOTTLE_CHATEAU_ROMANI:
        case RI_BOTTLE_MILK:
        case RI_BOTTLE_GOLD_DUST:
        case RI_BOTTLE_RED_POTION:
            if (hasObtainedCheck) {
                return false;
            }
            for (s32 slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
                if (s.items[slot] == ITEM_NONE) {
                    return true;
                }
            }
            return false;
        case RI_MOONS_TEAR:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_MOONS_TEAR) && S_INV_CONTENT(s, ITEM_MOONS_TEAR) != ITEM_NONE);
        case RI_DEED_LAND:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_DEED_LAND) && S_INV_CONTENT(s, ITEM_DEED_LAND) != ITEM_NONE);
        case RI_DEED_SWAMP:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_DEED_SWAMP) && S_INV_CONTENT(s, ITEM_DEED_SWAMP) != ITEM_NONE);
        case RI_DEED_MOUNTAIN:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_DEED_MOUNTAIN) &&
                     S_INV_CONTENT(s, ITEM_DEED_MOUNTAIN) != ITEM_NONE);
        case RI_DEED_OCEAN:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_DEED_OCEAN) && S_INV_CONTENT(s, ITEM_DEED_OCEAN) != ITEM_NONE);
        case RI_ROOM_KEY:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_ROOM_KEY) && S_INV_CONTENT(s, ITEM_ROOM_KEY) != ITEM_NONE);
        case RI_LETTER_TO_MAMA:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_MAMA) &&
                     S_INV_CONTENT(s, ITEM_LETTER_MAMA) != ITEM_NONE);
        case RI_LETTER_TO_KAFEI:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_KAFEI) &&
                     S_INV_CONTENT(s, ITEM_LETTER_TO_KAFEI) != ITEM_NONE);
        case RI_PENDANT_OF_MEMORIES:
            return !(GetRandoInf(s, RANDO_INF_OBTAINED_PENDANT_OF_MEMORIES) &&
                     S_INV_CONTENT(s, ITEM_PENDANT_OF_MEMORIES) != ITEM_NONE);
        case RI_DOUBLE_DEFENSE:
            return !s.doubleDefense;
        case RI_GREAT_SPIN_ATTACK:
            return !CheckWeekEventReg(s, WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
        case RI_WOODFALL_BOSS_KEY:
            return !CheckDungeonItem(s, DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
        case RI_WOODFALL_COMPASS:
            return !CheckDungeonItem(s, DUNGEON_COMPASS, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
        case RI_WOODFALL_MAP:
            return !CheckDungeonItem(s, DUNGEON_MAP, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
        case RI_SNOWHEAD_BOSS_KEY:
            return !CheckDungeonItem(s, DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
        case RI_SNOWHEAD_COMPASS:
            return !CheckDungeonItem(s, DUNGEON_COMPASS, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
        case RI_SNOWHEAD_MAP:
            return !CheckDungeonItem(s, DUNGEON_MAP, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
        case RI_GREAT_BAY_BOSS_KEY:
            return !CheckDungeonItem(s, DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
        case RI_GREAT_BAY_COMPASS:
            return !CheckDungeonItem(s, DUNGEON_COMPASS, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
        case RI_GREAT_BAY_MAP:
            return !CheckDungeonItem(s, DUNGEON_MAP, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
        case RI_STONE_TOWER_BOSS_KEY:
            return !CheckDungeonItem(s, DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
        case RI_STONE_TOWER_COMPASS:
            return !CheckDungeonItem(s, DUNGEON_COMPASS, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
        case RI_STONE_TOWER_MAP:
            return !CheckDungeonItem(s, DUNGEON_MAP, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
        case RI_OWL_CLOCK_TOWN_SOUTH:
            return !CanOwlWarp(s, OWL_WARP_CLOCK_TOWN);
        case RI_OWL_GREAT_BAY_COAST:
            return !CanOwlWarp(s, OWL_WARP_GREAT_BAY_COAST);
        case RI_OWL_IKANA_CANYON:
            return !CanOwlWarp(s, OWL_WARP_IKANA_CANYON);
        case RI_OWL_MILK_ROAD:
            return !CanOwlWarp(s, OWL_WARP_MILK_ROAD);
        case RI_OWL_MOUNTAIN_VILLAGE:
            return !CanOwlWarp(s, OWL_WARP_MOUNTAIN_VILLAGE);
        case RI_OWL_SNOWHEAD:
            return !CanOwlWarp(s, OWL_WARP_SNOWHEAD);
        case RI_OWL_SOUTHERN_SWAMP:
            return !CanOwlWarp(s, OWL_WARP_SOUTHERN_SWAMP);
        case RI_OWL_STONE_TOWER:
            return !CanOwlWarp(s, OWL_WARP_STONE_TOWER);
        case RI_OWL_WOODFALL:
            return !CanOwlWarp(s, OWL_WARP_WOODFALL);
        case RI_OWL_ZORA_CAPE:
            return !CanOwlWarp(s, OWL_WARP_ZORA_CAPE);
        case RI_BOMBERS_NOTEBOOK:
            return !CheckQuestItem(s, QUEST_BOMBERS_NOTEBOOK);
        case RI_REMAINS_ODOLWA:
            return !CheckQuestItem(s, QUEST_REMAINS_ODOLWA);
        case RI_REMAINS_GOHT:
            return !CheckQuestItem(s, QUEST_REMAINS_GOHT);
        case RI_REMAINS_GYORG:
            return !CheckQuestItem(s, QUEST_REMAINS_GYORG);
        case RI_REMAINS_TWINMOLD:
            return !CheckQuestItem(s, QUEST_REMAINS_TWINMOLD);
        case RI_SONG_DOUBLE_TIME:
            return !GetRandoInf(s, RANDO_INF_OBTAINED_SONG_DOUBLE_TIME);
        case RI_SONG_NOVA:
            return !CheckQuestItem(s, QUEST_SONG_BOSSA_NOVA);
        case RI_SONG_ELEGY:
            return !CheckQuestItem(s, QUEST_SONG_ELEGY);
        case RI_SONG_EPONA:
            return !CheckQuestItem(s, QUEST_SONG_EPONA);
        case RI_SONG_HEALING:
            return !CheckQuestItem(s, QUEST_SONG_HEALING);
        case RI_SONG_INVERTED_TIME:
            return !GetRandoInf(s, RANDO_INF_OBTAINED_SONG_INVERTED_TIME);
        case RI_SONG_LULLABY_INTRO:
            return !CheckQuestItem(s, QUEST_SONG_LULLABY_INTRO);
        case RI_SONG_LULLABY:
            return !CheckQuestItem(s, QUEST_SONG_LULLABY);
        case RI_SONG_OATH:
            return !CheckQuestItem(s, QUEST_SONG_OATH);
        case RI_SONG_SARIA:
            if (hasObtainedCheck) {
                return false;
            }
            return true;
        case RI_SONG_SOARING:
            return !CheckQuestItem(s, QUEST_SONG_SOARING);
        case RI_SONG_SONATA:
            return !CheckQuestItem(s, QUEST_SONG_SONATA);
        case RI_SONG_STORMS:
            return !CheckQuestItem(s, QUEST_SONG_STORMS);
        case RI_SONG_SUN:
            return !CheckQuestItem(s, QUEST_SONG_SUN);
        case RI_SONG_TIME:
            return !CheckQuestItem(s, QUEST_SONG_TIME);
        case RI_TINGLE_MAP_CLOCK_TOWN:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_CLOCK_TOWN);
        case RI_TINGLE_MAP_WOODFALL:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_WOODFALL);
        case RI_TINGLE_MAP_GREAT_BAY:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_GREAT_BAY);
        case RI_TINGLE_MAP_ROMANI_RANCH:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_ROMANI_RANCH);
        case RI_TINGLE_MAP_SNOWHEAD:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_SNOWHEAD);
        case RI_TINGLE_MAP_STONE_TOWER:
            return !CheckWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_STONE_TOWER);
        LS_SOUL_CASES:
            return !GetRandoInf(s, SOUL_RI_TO_RANDO_INF(randoItemId));
        case RI_ABILITY_SWIM:
            return !GetRandoInf(s, RANDO_INF_OBTAINED_SWIM);
        case RI_FROG_BLUE:
            return !CheckWeekEventReg(s, WEEKEVENTREG_33_01);
        case RI_FROG_CYAN:
            return !CheckWeekEventReg(s, WEEKEVENTREG_32_40);
        case RI_FROG_PINK:
            return !CheckWeekEventReg(s, WEEKEVENTREG_32_80);
        case RI_FROG_WHITE:
            return !CheckWeekEventReg(s, WEEKEVENTREG_33_02);
        case RI_TIME_DAY_1:
        case RI_TIME_NIGHT_1:
        case RI_TIME_DAY_2:
        case RI_TIME_NIGHT_2:
        case RI_TIME_DAY_3:
        case RI_TIME_NIGHT_3:
            return !GetRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 +
                                       Rando::ClockItems::GetHalfDayIndexFromClockItem(randoItemId));
        case RI_TIME_PROGRESSIVE:
            if (hasObtainedCheck) {
                return false;
            } else if (AllOwnedHalfDaysMask(s) == 0x3F) {
                return false;
            }
            return true;
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_UP:
            return !GetRandoInf(s, RANDO_INF_OBTAINED_OCARINA_BUTTON_A + (randoItemId - RI_OCARINA_BUTTON_A));
        case RI_MASK_ALL_NIGHT:
        case RI_MASK_BLAST:
        case RI_MASK_BREMEN:
        case RI_MASK_BUNNY:
        case RI_MASK_CAPTAIN:
        case RI_MASK_CIRCUS_LEADER:
        case RI_MASK_COUPLE:
        case RI_MASK_DEKU:
        case RI_MASK_DON_GERO:
        case RI_MASK_FIERCE_DEITY:
        case RI_MASK_GARO:
        case RI_MASK_GIANT:
        case RI_MASK_GIBDO:
        case RI_MASK_GORON:
        case RI_MASK_GREAT_FAIRY:
        case RI_MASK_KAFEIS_MASK:
        case RI_MASK_KAMARO:
        case RI_MASK_KEATON:
        case RI_MASK_POSTMAN:
        case RI_MASK_ROMANI:
        case RI_MASK_SCENTS:
        case RI_MASK_STONE:
        case RI_MASK_TRUTH:
        case RI_MASK_ZORA: {
            ItemId itemId = Rando::StaticData::Items[randoItemId].itemId;
            return S_INV_CONTENT(s, itemId) != itemId;
        }
#ifdef DIPTYCH_GAME_MODULE
        case RI_DIPTYCH_FOREIGN:
            return !hasObtainedCheck;
#endif
        default:
            break;
    }

    if (vanillaCantObtain()) {
        return false;
    }

    return true;
}

RandoItemId Resolve(const State& s, RandoItemId randoItemId, bool checkObtained) {
    if (IsObtainable(s, randoItemId, checkObtained)) {
        switch (randoItemId) {
            case RI_TIME_PROGRESSIVE: {
                int mode = s.options[RO_CLOCK_SHUFFLE_PROGRESSIVE];
                if (mode == RO_CLOCK_SHUFFLE_RANDOM) {
                    return RI_JUNK;
                }
                RandoItemId ascending[] = { RI_TIME_DAY_1,   RI_TIME_NIGHT_1, RI_TIME_DAY_2,
                                            RI_TIME_NIGHT_2, RI_TIME_DAY_3,   RI_TIME_NIGHT_3 };
                RandoItemId descending[] = { RI_TIME_NIGHT_3, RI_TIME_DAY_3,   RI_TIME_NIGHT_2,
                                             RI_TIME_DAY_2,   RI_TIME_NIGHT_1, RI_TIME_DAY_1 };
                RandoItemId* order = (mode == RO_CLOCK_SHUFFLE_DESCENDING) ? descending : ascending;
                for (int i = 0; i < 6; ++i) {
                    int halfIndex = Rando::ClockItems::GetHalfDayIndexFromClockItem(order[i]);
                    if (halfIndex >= 0 && !GetRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 + halfIndex)) {
                        return order[i];
                    }
                }
                return RI_JUNK;
            }
            case RI_PROGRESSIVE_BOMB_BAG:
                if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
                    return RI_BOMB_BAG_20;
                } else if (CurUpgValue(s, UPG_BOMB_BAG) == 1) {
                    return RI_BOMB_BAG_30;
                } else if (CurUpgValue(s, UPG_BOMB_BAG) == 2) {
                    return RI_BOMB_BAG_40;
                }
                return RI_JUNK;
            case RI_PROGRESSIVE_BOW:
                if (CurUpgValue(s, UPG_QUIVER) == 0) {
                    return RI_BOW;
                } else if (CurUpgValue(s, UPG_QUIVER) == 1) {
                    return RI_QUIVER_40;
                } else if (CurUpgValue(s, UPG_QUIVER) == 2) {
                    return RI_QUIVER_50;
                }
                return RI_JUNK;
            case RI_PROGRESSIVE_LULLABY:
                if (!CheckQuestItem(s, QUEST_SONG_LULLABY_INTRO)) {
                    return RI_SONG_LULLABY_INTRO;
                } else if (!CheckQuestItem(s, QUEST_SONG_LULLABY)) {
                    return RI_SONG_LULLABY;
                }
                return RI_JUNK;
            case RI_PROGRESSIVE_MAGIC:
                if (!s.isMagicAcquired) {
                    return RI_SINGLE_MAGIC;
                } else if (!s.isDoubleMagicAcquired) {
                    return RI_DOUBLE_MAGIC;
                }
                return RI_JUNK;
            case RI_PROGRESSIVE_WALLET:
                if (CurUpgValue(s, UPG_WALLET) == 0) {
                    return RI_WALLET_ADULT;
                } else if (CurUpgValue(s, UPG_WALLET) == 1) {
                    return RI_WALLET_GIANT;
                } else if (CurUpgValue(s, UPG_WALLET) == 2) {
                    return RI_WALLET_TYCOON;
                }
                return RI_JUNK;
            case RI_PROGRESSIVE_SWORD:
                if (CurEquipValue(s, EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE &&
                    (StolenItem1(s) < ITEM_SWORD_KOKIRI) && (StolenItem2(s) < ITEM_SWORD_KOKIRI)) {
                    return RI_SWORD_KOKIRI;
                } else if (CurEquipValue(s, EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI ||
                           (StolenItem1(s) == ITEM_SWORD_KOKIRI) || (StolenItem2(s) == ITEM_SWORD_KOKIRI)) {
                    return RI_SWORD_RAZOR;
                } else if (CurEquipValue(s, EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR ||
                           (StolenItem1(s) == ITEM_SWORD_RAZOR) || (StolenItem2(s) == ITEM_SWORD_RAZOR)) {
                    return RI_SWORD_GILDED;
                }
                return RI_JUNK;
            default:
                break;
        }
        return randoItemId;
    } else {
        switch (randoItemId) {
            case RI_BOTTLE_GOLD_DUST:
                if (HasEmptyBottle(s)) {
                    return RI_GOLD_DUST_REFILL;
                }
                break;
            case RI_BOTTLE_MILK:
                if (HasEmptyBottle(s)) {
                    return RI_MILK_REFILL;
                }
                break;
            case RI_BOTTLE_CHATEAU_ROMANI:
                if (HasEmptyBottle(s)) {
                    return RI_CHATEAU_ROMANI_REFILL;
                }
                break;
            case RI_BOTTLE_RED_POTION:
                if (HasEmptyBottle(s)) {
                    return RI_RED_POTION_REFILL;
                }
            default:
                break;
        }
        return RI_JUNK;
    }
}

}

}
