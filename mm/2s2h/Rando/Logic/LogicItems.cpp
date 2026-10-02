#include "LogicStateFields.h"
#include "Rando/ActorBehavior/Souls.h"

namespace Rando {

namespace Logic {

namespace {

void GiveVanillaItem(State& s, u8 item) {
    s16 i;

    if (item == ITEM_SHIP) {
        return;
    }

    if (item == ITEM_SKULL_TOKEN) {
        SetQuestItem(s, item - ITEM_SKULL_TOKEN + QUEST_QUIVER);
        IncrementSkullTokenCount(s, SCENE_KINDAN2);
        return;

    } else if (item == ITEM_TINGLE_MAP) {
        return;

    } else if (item == ITEM_BOMBERS_NOTEBOOK) {
        SetQuestItem(s, QUEST_BOMBERS_NOTEBOOK);
        return;

    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        s.questItems += (1 << QUEST_HEART_PIECE_COUNT);
        if ((s.questItems & 0xF0000000) == (4 << QUEST_HEART_PIECE_COUNT)) {
            s.questItems ^= (4 << QUEST_HEART_PIECE_COUNT);
            s.healthCapacity += 0x10;
        }
        return;

    } else if (item == ITEM_HEART_CONTAINER) {
        s.healthCapacity += 0x10;
        return;

    } else if ((item >= ITEM_SONG_SONATA) && (item <= ITEM_SONG_LULLABY_INTRO)) {
        SetQuestItem(s, item - ITEM_SONG_SONATA + QUEST_SONG_SONATA);
        return;

    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED)) {
        SetEquipValue(s, EQUIP_TYPE_SWORD, item - ITEM_SWORD_KOKIRI + EQUIP_VALUE_SWORD_KOKIRI);
        return;

    } else if ((item >= ITEM_SHIELD_HERO) && (item <= ITEM_SHIELD_MIRROR)) {
        if (CurEquipValue(s, EQUIP_TYPE_SHIELD) != (u16)(item - ITEM_SHIELD_HERO + EQUIP_VALUE_SHIELD_HERO)) {
            SetEquipValue(s, EQUIP_TYPE_SHIELD, item - ITEM_SHIELD_HERO + EQUIP_VALUE_SHIELD_HERO);
        }
        return;

    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP) ||
               (item == ITEM_KEY_SMALL)) {
        return;

    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CurUpgValue(s, UPG_QUIVER) == 0) {
            ChangeUpgrade(s, UPG_QUIVER, 1);
            S_INV_CONTENT(s, ITEM_BOW) = ITEM_BOW;
            return;
        }

    } else if (item == ITEM_QUIVER_40) {
        ChangeUpgrade(s, UPG_QUIVER, 2);
        S_INV_CONTENT(s, ITEM_BOW) = ITEM_BOW;
        return;

    } else if (item == ITEM_QUIVER_50) {
        ChangeUpgrade(s, UPG_QUIVER, 3);
        S_INV_CONTENT(s, ITEM_BOW) = ITEM_BOW;
        return;

    } else if (item == ITEM_BOMB_BAG_20) {
        if (CurUpgValue(s, UPG_BOMB_BAG) == 0) {
            ChangeUpgrade(s, UPG_BOMB_BAG, 1);
            S_INV_CONTENT(s, ITEM_BOMB) = ITEM_BOMB;
            return;
        }

    } else if (item == ITEM_BOMB_BAG_30) {
        ChangeUpgrade(s, UPG_BOMB_BAG, 2);
        S_INV_CONTENT(s, ITEM_BOMB) = ITEM_BOMB;
        return;

    } else if (item == ITEM_BOMB_BAG_40) {
        ChangeUpgrade(s, UPG_BOMB_BAG, 3);
        S_INV_CONTENT(s, ITEM_BOMB) = ITEM_BOMB;
        return;

    } else if (item == ITEM_WALLET_ADULT) {
        ChangeUpgrade(s, UPG_WALLET, 1);
        return;

    } else if (item == ITEM_WALLET_GIANT) {
        ChangeUpgrade(s, UPG_WALLET, 2);
        return;

    } else if (item == ITEM_DEKU_STICK_UPGRADE_20) {
        S_INV_CONTENT(s, ITEM_DEKU_STICK) = ITEM_DEKU_STICK;
        ChangeUpgrade(s, UPG_DEKU_STICKS, 2);
        return;

    } else if (item == ITEM_DEKU_STICK_UPGRADE_30) {
        S_INV_CONTENT(s, ITEM_DEKU_STICK) = ITEM_DEKU_STICK;
        ChangeUpgrade(s, UPG_DEKU_STICKS, 3);
        return;

    } else if (item == ITEM_DEKU_NUT_UPGRADE_30) {
        S_INV_CONTENT(s, ITEM_DEKU_NUT) = ITEM_DEKU_NUT;
        ChangeUpgrade(s, UPG_DEKU_NUTS, 2);
        return;

    } else if (item == ITEM_DEKU_NUT_UPGRADE_40) {
        S_INV_CONTENT(s, ITEM_DEKU_NUT) = ITEM_DEKU_NUT;
        ChangeUpgrade(s, UPG_DEKU_NUTS, 3);
        return;

    } else if (item == ITEM_DEKU_STICK) {
        if (S_INV_CONTENT(s, ITEM_DEKU_STICK) != ITEM_DEKU_STICK) {
            ChangeUpgrade(s, UPG_DEKU_STICKS, 1);
        }

    } else if ((item == ITEM_DEKU_STICKS_5) || (item == ITEM_DEKU_STICKS_10)) {
        if (S_INV_CONTENT(s, ITEM_DEKU_STICK) != ITEM_DEKU_STICK) {
            ChangeUpgrade(s, UPG_DEKU_STICKS, 1);
        }
        item = ITEM_DEKU_STICK;

    } else if (item == ITEM_DEKU_NUT) {
        if (S_INV_CONTENT(s, ITEM_DEKU_NUT) != ITEM_DEKU_NUT) {
            ChangeUpgrade(s, UPG_DEKU_NUTS, 1);
        }

    } else if ((item == ITEM_DEKU_NUTS_5) || (item == ITEM_DEKU_NUTS_10)) {
        if (S_INV_CONTENT(s, ITEM_DEKU_NUT) != ITEM_DEKU_NUT) {
            ChangeUpgrade(s, UPG_DEKU_NUTS, 1);
        }
        item = ITEM_DEKU_NUT;

    } else if (item == ITEM_POWDER_KEG) {
        S_INV_CONTENT(s, ITEM_POWDER_KEG) = ITEM_POWDER_KEG;
        return;

    } else if (item == ITEM_BOMB) {
        return;

    } else if ((item >= ITEM_BOMBS_5) && (item <= ITEM_BOMBS_30)) {
        if (s.items[SLOT_BOMB] != ITEM_BOMB) {
            S_INV_CONTENT(s, ITEM_BOMB) = ITEM_BOMB;
        }
        return;

    } else if (item == ITEM_BOMBCHU) {
        S_INV_CONTENT(s, ITEM_BOMBCHU) = ITEM_BOMBCHU;
        return;

    } else if ((item >= ITEM_BOMBCHUS_20) && (item <= ITEM_BOMBCHUS_5)) {
        if (s.items[SLOT_BOMBCHU] != ITEM_BOMBCHU) {
            S_INV_CONTENT(s, ITEM_BOMBCHU) = ITEM_BOMBCHU;
        }
        return;

    } else if ((item >= ITEM_ARROWS_10) && (item <= ITEM_ARROWS_50)) {
        return;

    } else if (item == ITEM_OCARINA_OF_TIME) {
        S_INV_CONTENT(s, ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
        return;

    } else if (item == ITEM_MAGIC_BEANS) {
        if (S_INV_CONTENT(s, ITEM_MAGIC_BEANS) == ITEM_NONE) {
            S_INV_CONTENT(s, item) = item;
        }
        return;

    } else if ((item >= ITEM_REMAINS_ODOLWA) && (item <= ITEM_REMAINS_TWINMOLD)) {
        SetQuestItem(s, item - ITEM_REMAINS_ODOLWA + QUEST_REMAINS_ODOLWA);
        return;

    } else if (item == ITEM_RECOVERY_HEART) {
        return;

    } else if ((item == ITEM_MAGIC_JAR_SMALL) || (item == ITEM_MAGIC_JAR_BIG)) {
        if (!CheckWeekEventReg(s, WEEKEVENTREG_12_80)) {
            SetWeekEventReg(s, WEEKEVENTREG_12_80);
        }
        return;

    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_RUPEE_HUGE)) {
        return;

    } else if (item == ITEM_LONGSHOT) {
        u8 slot = SLOT(item);
        for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
            if (s.items[slot + i] == ITEM_NONE) {
                s.items[slot + i] = ITEM_POTION_RED;
                return;
            }
        }
        return;

    } else if ((item == ITEM_MILK_BOTTLE) || (item == ITEM_POE) || (item == ITEM_GOLD_DUST) || (item == ITEM_CHATEAU) ||
               (item == ITEM_HYLIAN_LOACH)) {
        u8 slot = SLOT(item);
        for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
            if (s.items[slot + i] == ITEM_NONE) {
                s.items[slot + i] = item;
                return;
            }
        }
        return;

    } else if (item == ITEM_BOTTLE) {
        u8 slot = SLOT(item);
        for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
            if (s.items[slot + i] == ITEM_NONE) {
                s.items[slot + i] = item;
                return;
            }
        }
        return;

    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_OBABA_DRINK)) || (item == ITEM_CHATEAU_2) ||
               (item == ITEM_MILK) || (item == ITEM_GOLD_DUST_2) || (item == ITEM_HYLIAN_LOACH_2) ||
               (item == ITEM_SEAHORSE_CAUGHT)) {
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
            u8 slot = SLOT(item);
            for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
                if (s.items[slot + i] == ITEM_BOTTLE) {
                    s.items[slot + i] = item;
                    return;
                }
            }
        } else {
            u8 slot = SLOT(item);
            for (i = BOTTLE_FIRST; i < BOTTLE_MAX; i++) {
                if (s.items[slot + i] == ITEM_NONE) {
                    s.items[slot + i] = item;
                    return;
                }
            }
        }
        // No free bottle falls through and overwrites the first slot, matching native behavior.

    } else if ((item >= ITEM_MOONS_TEAR) && (item <= ITEM_MASK_GIANT)) {
        S_INV_CONTENT(s, item) = item;
        return;
    }

    // Native Item_GiveImpl indexes past gItemSlots for owned upgrades; skip that write.
    if (item < ARRAY_COUNT(gItemSlots)) {
        S_INV_CONTENT(s, item) = item;
    }
}

}

void Apply(State& s, RandoItemId randoItemId) {
    switch (randoItemId) {
        case RI_CLOCK_TOWN_STRAY_FAIRY:
            SetWeekEventReg(s, WEEKEVENTREG_08_80);
            break;
        case RI_WOODFALL_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE]++;
            break;
        case RI_SNOWHEAD_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE]++;
            break;
        case RI_GREAT_BAY_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE]++;
            break;
        case RI_STONE_TOWER_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE]++;
            break;
        case RI_GREAT_SPIN_ATTACK:
            SetWeekEventReg(s, WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
            break;
        case RI_DOUBLE_DEFENSE:
            s.doubleDefense = true;
            break;
        case RI_SINGLE_MAGIC:
            s.isMagicAcquired = true;
            SetWeekEventReg(s, WEEKEVENTREG_12_80);
            break;
        case RI_DOUBLE_MAGIC:
            s.isMagicAcquired = true;
            s.isDoubleMagicAcquired = true;
            SetWeekEventReg(s, WEEKEVENTREG_12_80);
            break;
        case RI_WOODFALL_BOSS_KEY:
        case RI_WOODFALL_MAP:
        case RI_WOODFALL_COMPASS:
            SetDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                           DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
            break;
        case RI_SNOWHEAD_BOSS_KEY:
        case RI_SNOWHEAD_MAP:
        case RI_SNOWHEAD_COMPASS:
            SetDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                           DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
            break;
        case RI_GREAT_BAY_BOSS_KEY:
        case RI_GREAT_BAY_MAP:
        case RI_GREAT_BAY_COMPASS:
            SetDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                           DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
            break;
        case RI_STONE_TOWER_BOSS_KEY:
        case RI_STONE_TOWER_MAP:
        case RI_STONE_TOWER_COMPASS:
            SetDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                           DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
            break;
        case RI_WOODFALL_SMALL_KEY:
        case RI_SNOWHEAD_SMALL_KEY:
        case RI_GREAT_BAY_SMALL_KEY:
        case RI_STONE_TOWER_SMALL_KEY: {
            s32 dungeon = randoItemId == RI_WOODFALL_SMALL_KEY    ? DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE
                          : randoItemId == RI_SNOWHEAD_SMALL_KEY  ? DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE
                          : randoItemId == RI_GREAT_BAY_SMALL_KEY ? DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE
                                                                  : DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
            if (s.dungeonKeys[dungeon] < 0) {
                s.dungeonKeys[dungeon] = 1;
                s.foundDungeonKeys[dungeon] = 1;
            } else {
                s.dungeonKeys[dungeon]++;
                s.foundDungeonKeys[dungeon]++;
            }
            break;
        }
        case RI_SKELETON_KEY: {
            const s8 maxKeys[] = { 1, 3, 1, 4 };
            for (s32 dungeon = DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE; dungeon <= DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
                 dungeon++) {
                if (s.dungeonKeys[dungeon] < maxKeys[dungeon]) {
                    s.dungeonKeys[dungeon] = maxKeys[dungeon];
                    s.foundDungeonKeys[dungeon] = maxKeys[dungeon];
                }
            }
            break;
        }
        case RI_TRIFORCE_PIECE:
        case RI_TRIFORCE_PIECE_PREVIOUS:
            s.foundTriforcePieces++;
            if (s.foundTriforcePieces == s.options[RO_TRIFORCE_PIECES_REQUIRED]) {
                if (!GetRandoInf(s, RANDO_INF_OBTAINED_SOUL_OF_BOSS_MAJORA)) {
                    Apply(s, RI_SOUL_BOSS_MAJORA);
                }
            }
            break;
        case RI_PROGRESSIVE_MAGIC:
        case RI_PROGRESSIVE_BOW:
        case RI_PROGRESSIVE_BOMB_BAG:
        case RI_PROGRESSIVE_LULLABY:
        case RI_PROGRESSIVE_SWORD:
        case RI_PROGRESSIVE_WALLET:
            Apply(s, Resolve(s, randoItemId));
            break;
        case RI_BOMB_BAG_20:
        case RI_BOMB_BAG_30:
        case RI_BOMB_BAG_40:
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            S_INV_CONTENT(s, ITEM_BOMBCHU) = ITEM_BOMBCHU;
            break;
        case RI_WALLET_ADULT:
        case RI_WALLET_GIANT:
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_WALLET_TYCOON:
            ChangeUpgrade(s, UPG_WALLET, 3);
            break;
        case RI_GS_TOKEN_SWAMP:
            // Match the native QUEST_QUIVER bug in z_parameter.c.
            SetQuestItem(s, QUEST_QUIVER);
            IncrementSkullTokenCount(s, SCENE_KINSTA1);
            break;
        case RI_GS_TOKEN_OCEAN:
            SetQuestItem(s, QUEST_QUIVER);
            IncrementSkullTokenCount(s, SCENE_KINDAN2);
            break;
        case RI_MOONS_TEAR:
            SetRandoInf(s, RANDO_INF_OBTAINED_MOONS_TEAR);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_DEED_LAND:
            SetRandoInf(s, RANDO_INF_OBTAINED_DEED_LAND);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_DEED_SWAMP:
            SetRandoInf(s, RANDO_INF_OBTAINED_DEED_SWAMP);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_DEED_MOUNTAIN:
            SetRandoInf(s, RANDO_INF_OBTAINED_DEED_MOUNTAIN);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_DEED_OCEAN:
            SetRandoInf(s, RANDO_INF_OBTAINED_DEED_OCEAN);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_ROOM_KEY:
            SetRandoInf(s, RANDO_INF_OBTAINED_ROOM_KEY);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_LETTER_TO_MAMA:
            SetRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_MAMA);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_LETTER_TO_KAFEI:
            SetRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_KAFEI);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_PENDANT_OF_MEMORIES:
            SetRandoInf(s, RANDO_INF_OBTAINED_PENDANT_OF_MEMORIES);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_POWDER_KEG:
            SetWeekEventReg(s, WEEKEVENTREG_HAS_POWDERKEG_PRIVILEGES);
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_SWORD_GILDED:
        case RI_SWORD_KOKIRI:
        case RI_SWORD_RAZOR:
            if (StolenItem1(s) == ITEM_SWORD_KOKIRI || StolenItem1(s) == ITEM_SWORD_RAZOR) {
                SetStolenItem1(s, STOLEN_ITEM_NONE);
            }
            if (StolenItem2(s) == ITEM_SWORD_KOKIRI || StolenItem2(s) == ITEM_SWORD_RAZOR) {
                SetStolenItem2(s, STOLEN_ITEM_NONE);
            }
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_TINGLE_MAP_CLOCK_TOWN:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_CLOCK_TOWN);
            break;
        case RI_TINGLE_MAP_WOODFALL:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_WOODFALL);
            break;
        case RI_TINGLE_MAP_SNOWHEAD:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_SNOWHEAD);
            break;
        case RI_TINGLE_MAP_ROMANI_RANCH:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_ROMANI_RANCH);
            break;
        case RI_TINGLE_MAP_GREAT_BAY:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_GREAT_BAY);
            break;
        case RI_TINGLE_MAP_STONE_TOWER:
            SetWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_STONE_TOWER);
            break;
        case RI_OWL_CLOCK_TOWN_SOUTH:
            ActivateOwl(s, OWL_WARP_CLOCK_TOWN);
            break;
        case RI_OWL_GREAT_BAY_COAST:
            ActivateOwl(s, OWL_WARP_GREAT_BAY_COAST);
            break;
        case RI_OWL_IKANA_CANYON:
            ActivateOwl(s, OWL_WARP_IKANA_CANYON);
            break;
        case RI_OWL_MILK_ROAD:
            ActivateOwl(s, OWL_WARP_MILK_ROAD);
            break;
        case RI_OWL_MOUNTAIN_VILLAGE:
            ActivateOwl(s, OWL_WARP_MOUNTAIN_VILLAGE);
            break;
        case RI_OWL_SNOWHEAD:
            ActivateOwl(s, OWL_WARP_SNOWHEAD);
            break;
        case RI_OWL_SOUTHERN_SWAMP:
            ActivateOwl(s, OWL_WARP_SOUTHERN_SWAMP);
            break;
        case RI_OWL_STONE_TOWER:
            ActivateOwl(s, OWL_WARP_STONE_TOWER);
            break;
        case RI_OWL_WOODFALL:
            ActivateOwl(s, OWL_WARP_WOODFALL);
            break;
        case RI_OWL_ZORA_CAPE:
            ActivateOwl(s, OWL_WARP_ZORA_CAPE);
            break;
        case RI_TIME_DAY_1:
        case RI_TIME_NIGHT_1:
        case RI_TIME_DAY_2:
        case RI_TIME_NIGHT_2:
        case RI_TIME_DAY_3:
        case RI_TIME_NIGHT_3: {
            int index = Rando::ClockItems::GetHalfDayIndexFromClockItem(randoItemId);
            if (index != Rando::ClockItems::INVALID) {
                SetRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 + index);
            }
            break;
        }
        case RI_TIME_PROGRESSIVE: {
            RandoItemId concrete = Resolve(s, RI_TIME_PROGRESSIVE);
            if (concrete != RI_JUNK) {
                Apply(s, concrete);
            }
            break;
        }
        case RI_HEART_CONTAINER:
        case RI_HEART_PIECE:
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_BOTTLE_RED_POTION:
            GiveVanillaItem(s, ITEM_LONGSHOT);
            break;
        case RI_SOUL_BOSS_GOHT:
        case RI_SOUL_BOSS_GYORG:
        case RI_SOUL_BOSS_MAJORA:
        case RI_SOUL_BOSS_ODOLWA:
        case RI_SOUL_BOSS_TWINMOLD:
        case RI_SOUL_ENEMY_ALIEN:
        case RI_SOUL_ENEMY_ARMOS:
        case RI_SOUL_ENEMY_BAD_BAT:
        case RI_SOUL_ENEMY_BEAMOS:
        case RI_SOUL_ENEMY_BOE:
        case RI_SOUL_ENEMY_BUBBLE:
        case RI_SOUL_ENEMY_CAPTAIN_KEETA:
        case RI_SOUL_ENEMY_CHUCHU:
        case RI_SOUL_ENEMY_DEATH_ARMOS:
        case RI_SOUL_ENEMY_DEEP_PYTHON:
        case RI_SOUL_ENEMY_DEKU_BABA:
        case RI_SOUL_ENEMY_DEXIHAND:
        case RI_SOUL_ENEMY_DINOLFOS:
        case RI_SOUL_ENEMY_DODONGO:
        case RI_SOUL_ENEMY_DRAGONFLY:
        case RI_SOUL_ENEMY_EENO:
        case RI_SOUL_ENEMY_EYEGORE:
        case RI_SOUL_ENEMY_FREEZARD:
        case RI_SOUL_ENEMY_GARO:
        case RI_SOUL_ENEMY_GEKKO:
        case RI_SOUL_ENEMY_GIANT_BEE:
        case RI_SOUL_ENEMY_GOMESS:
        case RI_SOUL_ENEMY_GUAY:
        case RI_SOUL_ENEMY_HIPLOOP:
        case RI_SOUL_ENEMY_IGOS_DU_IKANA:
        case RI_SOUL_ENEMY_IRON_KNUCKLE:
        case RI_SOUL_ENEMY_KEESE:
        case RI_SOUL_ENEMY_LEEVER:
        case RI_SOUL_ENEMY_LIKE_LIKE:
        case RI_SOUL_ENEMY_MAD_SCRUB:
        case RI_SOUL_ENEMY_NEJIRON:
        case RI_SOUL_ENEMY_OCTOROK:
        case RI_SOUL_ENEMY_PEAHAT:
        case RI_SOUL_ENEMY_PIRATE:
        case RI_SOUL_ENEMY_POE:
        case RI_SOUL_ENEMY_REDEAD:
        case RI_SOUL_ENEMY_SHELLBLADE:
        case RI_SOUL_ENEMY_SKULLFISH:
        case RI_SOUL_ENEMY_SKULLTULA:
        case RI_SOUL_ENEMY_SNAPPER:
        case RI_SOUL_ENEMY_STALCHILD:
        case RI_SOUL_ENEMY_TAKKURI:
        case RI_SOUL_ENEMY_TEKTITE:
        case RI_SOUL_ENEMY_WALLMASTER:
        case RI_SOUL_ENEMY_WART:
        case RI_SOUL_ENEMY_WIZROBE:
        case RI_SOUL_ENEMY_WOLFOS:
            SetRandoInf(s, SOUL_RI_TO_RANDO_INF(randoItemId));
            break;
        case RI_FROG_BLUE:
            SetWeekEventReg(s, WEEKEVENTREG_33_01);
            break;
        case RI_FROG_CYAN:
            SetWeekEventReg(s, WEEKEVENTREG_32_40);
            break;
        case RI_FROG_PINK:
            SetWeekEventReg(s, WEEKEVENTREG_32_80);
            break;
        case RI_FROG_WHITE:
            SetWeekEventReg(s, WEEKEVENTREG_33_02);
            break;
        case RI_ABILITY_SWIM:
            SetRandoInf(s, RANDO_INF_OBTAINED_SWIM);
            break;
        case RI_TRAP:
            break;
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_UP:
            SetRandoInf(s, RANDO_INF_OBTAINED_OCARINA_BUTTON_A + (randoItemId - RI_OCARINA_BUTTON_A));
            break;
        case RI_SONG_DOUBLE_TIME:
            SetRandoInf(s, RANDO_INF_OBTAINED_SONG_DOUBLE_TIME);
            break;
        case RI_SONG_INVERTED_TIME:
            SetRandoInf(s, RANDO_INF_OBTAINED_SONG_INVERTED_TIME);
            break;
        case RI_SONG_SARIA:
            s.sariaHintsAvailable++;
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
        case RI_JUNK:
        case RI_NONE:
            break;
#ifdef DIPTYCH_GAME_MODULE
        case RI_DIPTYCH_FOREIGN:
            break;
#endif
        default:
            GiveVanillaItem(s, Rando::StaticData::Items[randoItemId].itemId);
            break;
    }
}

void Remove(State& s, RandoItemId randoItemId) {
    switch (randoItemId) {
        case RI_CLOCK_TOWN_STRAY_FAIRY:
            ClearWeekEventReg(s, WEEKEVENTREG_08_80);
            break;
        case RI_WOODFALL_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE]--;
            break;
        case RI_SNOWHEAD_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE]--;
            break;
        case RI_GREAT_BAY_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE]--;
            break;
        case RI_STONE_TOWER_STRAY_FAIRY:
            s.strayFairies[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE]--;
            break;
        case RI_GREAT_SPIN_ATTACK:
            ClearWeekEventReg(s, WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
            break;
        case RI_DOUBLE_DEFENSE:
            s.doubleDefense = false;
            break;
        case RI_SINGLE_MAGIC:
            s.isMagicAcquired = false;
            s.isDoubleMagicAcquired = false;
            ClearWeekEventReg(s, WEEKEVENTREG_12_80);
            break;
        case RI_DOUBLE_MAGIC:
            s.isDoubleMagicAcquired = false;
            break;
        case RI_WOODFALL_BOSS_KEY:
        case RI_WOODFALL_MAP:
        case RI_WOODFALL_COMPASS:
            RemoveDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                              DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
            break;
        case RI_WOODFALL_SMALL_KEY:
            s.dungeonKeys[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE]--;
            s.foundDungeonKeys[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE]--;
            break;
        case RI_SNOWHEAD_BOSS_KEY:
        case RI_SNOWHEAD_MAP:
        case RI_SNOWHEAD_COMPASS:
            RemoveDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                              DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
            break;
        case RI_SNOWHEAD_SMALL_KEY:
            s.dungeonKeys[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE]--;
            s.foundDungeonKeys[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE]--;
            break;
        case RI_GREAT_BAY_BOSS_KEY:
        case RI_GREAT_BAY_MAP:
        case RI_GREAT_BAY_COMPASS:
            RemoveDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                              DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
            break;
        case RI_GREAT_BAY_SMALL_KEY:
            s.dungeonKeys[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE]--;
            s.foundDungeonKeys[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE]--;
            break;
        case RI_STONE_TOWER_BOSS_KEY:
        case RI_STONE_TOWER_MAP:
        case RI_STONE_TOWER_COMPASS:
            RemoveDungeonItem(s, Rando::StaticData::Items[randoItemId].itemId - ITEM_KEY_BOSS,
                              DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
            break;
        case RI_STONE_TOWER_SMALL_KEY:
            s.dungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE]--;
            s.foundDungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE]--;
            break;
        case RI_PROGRESSIVE_MAGIC:
            if (s.isDoubleMagicAcquired) {
                Remove(s, RI_DOUBLE_MAGIC);
            } else if (s.isMagicAcquired) {
                Remove(s, RI_SINGLE_MAGIC);
            }
            break;
        case RI_BOW:
            ChangeUpgrade(s, UPG_QUIVER, 0);
            DeleteItem(s, SLOT(ITEM_BOW));
            break;
        case RI_QUIVER_40:
            ChangeUpgrade(s, UPG_QUIVER, 1);
            break;
        case RI_QUIVER_50:
            ChangeUpgrade(s, UPG_QUIVER, 2);
            break;
        case RI_PROGRESSIVE_BOW:
            if (CurUpgValue(s, UPG_QUIVER) >= 3) {
                Remove(s, RI_QUIVER_50);
            } else if (CurUpgValue(s, UPG_QUIVER) >= 2) {
                Remove(s, RI_QUIVER_40);
            } else if (CurUpgValue(s, UPG_QUIVER) >= 1) {
                Remove(s, RI_BOW);
            }
            break;
        case RI_BOMB_BAG_20:
            ChangeUpgrade(s, UPG_BOMB_BAG, 0);
            DeleteItem(s, SLOT(ITEM_BOMB));
            DeleteItem(s, SLOT(ITEM_BOMBCHU));
            break;
        case RI_BOMB_BAG_30:
            ChangeUpgrade(s, UPG_BOMB_BAG, 1);
            break;
        case RI_BOMB_BAG_40:
            ChangeUpgrade(s, UPG_BOMB_BAG, 2);
            break;
        case RI_PROGRESSIVE_BOMB_BAG:
            if (CurUpgValue(s, UPG_BOMB_BAG) >= 3) {
                Remove(s, RI_BOMB_BAG_40);
            } else if (CurUpgValue(s, UPG_BOMB_BAG) >= 2) {
                Remove(s, RI_BOMB_BAG_30);
            } else if (CurUpgValue(s, UPG_BOMB_BAG) >= 1) {
                Remove(s, RI_BOMB_BAG_20);
            }
            break;
        case RI_WALLET_ADULT:
            ChangeUpgrade(s, UPG_WALLET, 0);
            break;
        case RI_WALLET_GIANT:
            ChangeUpgrade(s, UPG_WALLET, 1);
            break;
        case RI_WALLET_TYCOON:
            ChangeUpgrade(s, UPG_WALLET, 2);
            break;
        case RI_PROGRESSIVE_WALLET:
            if (CurUpgValue(s, UPG_WALLET) >= 3) {
                Remove(s, RI_WALLET_TYCOON);
            } else if (CurUpgValue(s, UPG_WALLET) >= 2) {
                Remove(s, RI_WALLET_GIANT);
            } else if (CurUpgValue(s, UPG_WALLET) >= 1) {
                Remove(s, RI_WALLET_ADULT);
            }
            break;
        case RI_SWORD_KOKIRI:
            SetEquipValue(s, EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
            break;
        case RI_SWORD_RAZOR:
            SetEquipValue(s, EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            break;
        case RI_SWORD_GILDED:
            SetEquipValue(s, EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_RAZOR);
            break;
        case RI_PROGRESSIVE_SWORD:
            if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_GILDED) {
                Remove(s, RI_SWORD_GILDED);
            } else if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_RAZOR) {
                Remove(s, RI_SWORD_RAZOR);
            } else if (CurEquipValue(s, EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_KOKIRI) {
                Remove(s, RI_SWORD_KOKIRI);
            }
            break;
        case RI_PROGRESSIVE_LULLABY:
            if (CheckQuestItem(s, QUEST_SONG_LULLABY)) {
                Remove(s, RI_SONG_LULLABY);
            } else if (CheckQuestItem(s, QUEST_SONG_LULLABY_INTRO)) {
                Remove(s, RI_SONG_LULLABY_INTRO);
            }
            break;
        case RI_SHIELD_MIRROR:
            SetEquipValue(s, EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HERO);
            break;
        case RI_SHIELD_HERO:
            if (CurEquipValue(s, EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HERO) {
                SetEquipValue(s, EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
            }
            break;
        case RI_GS_TOKEN_SWAMP: {
            int skullTokenCount = SkullTokenCount(s, SCENE_KINSTA1);
            skullTokenCount--;
            s.skullTokenCount = ((int)(skullTokenCount & 0xFFFF) << 0x10) | (s.skullTokenCount & 0xFFFF);
            break;
        }
        case RI_GS_TOKEN_OCEAN: {
            int skullTokenCount = SkullTokenCount(s, SCENE_KINDAN2);
            skullTokenCount--;
            s.skullTokenCount = (s.skullTokenCount & 0xFFFF0000) | (skullTokenCount & 0xFFFF);
            break;
        }
        case RI_MOONS_TEAR:
            ClearRandoInf(s, RANDO_INF_OBTAINED_MOONS_TEAR);
            break;
        case RI_DEED_LAND:
            ClearRandoInf(s, RANDO_INF_OBTAINED_DEED_LAND);
            break;
        case RI_DEED_SWAMP:
            ClearRandoInf(s, RANDO_INF_OBTAINED_DEED_SWAMP);
            break;
        case RI_DEED_MOUNTAIN:
            ClearRandoInf(s, RANDO_INF_OBTAINED_DEED_MOUNTAIN);
            break;
        case RI_DEED_OCEAN:
            ClearRandoInf(s, RANDO_INF_OBTAINED_DEED_OCEAN);
            break;
        case RI_ROOM_KEY:
            ClearRandoInf(s, RANDO_INF_OBTAINED_ROOM_KEY);
            break;
        case RI_LETTER_TO_MAMA:
            ClearRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_MAMA);
            break;
        case RI_LETTER_TO_KAFEI:
            ClearRandoInf(s, RANDO_INF_OBTAINED_LETTER_TO_KAFEI);
            break;
        case RI_PENDANT_OF_MEMORIES:
            ClearRandoInf(s, RANDO_INF_OBTAINED_PENDANT_OF_MEMORIES);
            break;
        case RI_OWL_CLOCK_TOWN_SOUTH:
            ClearOwl(s, OWL_WARP_CLOCK_TOWN);
            break;
        case RI_OWL_GREAT_BAY_COAST:
            ClearOwl(s, OWL_WARP_GREAT_BAY_COAST);
            break;
        case RI_OWL_IKANA_CANYON:
            ClearOwl(s, OWL_WARP_IKANA_CANYON);
            break;
        case RI_OWL_MILK_ROAD:
            ClearOwl(s, OWL_WARP_MILK_ROAD);
            break;
        case RI_OWL_MOUNTAIN_VILLAGE:
            ClearOwl(s, OWL_WARP_MOUNTAIN_VILLAGE);
            break;
        case RI_OWL_SNOWHEAD:
            ClearOwl(s, OWL_WARP_SNOWHEAD);
            break;
        case RI_OWL_SOUTHERN_SWAMP:
            ClearOwl(s, OWL_WARP_SOUTHERN_SWAMP);
            break;
        case RI_OWL_STONE_TOWER:
            ClearOwl(s, OWL_WARP_STONE_TOWER);
            break;
        case RI_OWL_WOODFALL:
            ClearOwl(s, OWL_WARP_WOODFALL);
            break;
        case RI_OWL_ZORA_CAPE:
            ClearOwl(s, OWL_WARP_ZORA_CAPE);
            break;
        case RI_TINGLE_MAP_CLOCK_TOWN:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_CLOCK_TOWN);
            break;
        case RI_TINGLE_MAP_WOODFALL:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_WOODFALL);
            break;
        case RI_TINGLE_MAP_SNOWHEAD:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_SNOWHEAD);
            break;
        case RI_TINGLE_MAP_ROMANI_RANCH:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_ROMANI_RANCH);
            break;
        case RI_TINGLE_MAP_GREAT_BAY:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_GREAT_BAY);
            break;
        case RI_TINGLE_MAP_STONE_TOWER:
            ClearWeekEventReg(s, WEEKEVENTREG_TINGLE_MAP_BOUGHT_STONE_TOWER);
            break;
        case RI_TIME_DAY_1:
        case RI_TIME_NIGHT_1:
        case RI_TIME_DAY_2:
        case RI_TIME_NIGHT_2:
        case RI_TIME_DAY_3:
        case RI_TIME_NIGHT_3: {
            int index = Rando::ClockItems::GetHalfDayIndexFromClockItem(randoItemId);
            if (index != Rando::ClockItems::INVALID) {
                ClearRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 + index);
            }
            break;
        }
        case RI_TIME_PROGRESSIVE: {
            const bool descending = (s.options[RO_CLOCK_SHUFFLE_PROGRESSIVE] == RO_CLOCK_SHUFFLE_DESCENDING);
            int toRemove = FindOwnedHalfDayIn(s, !descending);
            if (toRemove >= 0) {
                ClearRandoInf(s, RANDO_INF_OBTAINED_CLOCK_DAY_1 + toRemove);
            }
            break;
        }
        case RI_HEART_CONTAINER:
            s.healthCapacity -= 0x10;
            break;
        case RI_HEART_PIECE:
            if (S_HEART_PIECE_COUNT(s) == 0) {
                s.questItems += (1 << QUEST_HEART_PIECE_COUNT);
                s.questItems += (1 << QUEST_HEART_PIECE_COUNT);
                s.questItems += (1 << QUEST_HEART_PIECE_COUNT);
                s.healthCapacity -= 0x10;
            } else {
                s.questItems -= (1 << QUEST_HEART_PIECE_COUNT);
            }
            break;
        case RI_BOMBERS_NOTEBOOK:
            RemoveQuestItem(s, QUEST_BOMBERS_NOTEBOOK);
            break;
        case RI_SONG_DOUBLE_TIME:
            ClearRandoInf(s, RANDO_INF_OBTAINED_SONG_DOUBLE_TIME);
            break;
        case RI_SONG_ELEGY:
            RemoveQuestItem(s, QUEST_SONG_ELEGY);
            break;
        case RI_SONG_EPONA:
            RemoveQuestItem(s, QUEST_SONG_EPONA);
            break;
        case RI_SONG_HEALING:
            RemoveQuestItem(s, QUEST_SONG_HEALING);
            break;
        case RI_SONG_INVERTED_TIME:
            ClearRandoInf(s, RANDO_INF_OBTAINED_SONG_INVERTED_TIME);
            break;
        case RI_SONG_LULLABY_INTRO:
            RemoveQuestItem(s, QUEST_SONG_LULLABY_INTRO);
            break;
        case RI_SONG_LULLABY:
            RemoveQuestItem(s, QUEST_SONG_LULLABY);
            break;
        case RI_SONG_NOVA:
            RemoveQuestItem(s, QUEST_SONG_BOSSA_NOVA);
            break;
        case RI_SONG_OATH:
            RemoveQuestItem(s, QUEST_SONG_OATH);
            break;
        case RI_SONG_SARIA:
            s.sariaHintsAvailable = MAX(s.sariaHintsAvailable - 1, 0);
            if (s.sariaHintsAvailable == 0) {
                RemoveQuestItem(s, QUEST_SONG_SARIA);
            }
            break;
        case RI_SONG_SOARING:
            RemoveQuestItem(s, QUEST_SONG_SOARING);
            break;
        case RI_SONG_SONATA:
            RemoveQuestItem(s, QUEST_SONG_SONATA);
            break;
        case RI_SONG_STORMS:
            RemoveQuestItem(s, QUEST_SONG_STORMS);
            break;
        case RI_SONG_SUN:
            RemoveQuestItem(s, QUEST_SONG_SUN);
            break;
        case RI_SONG_TIME:
            RemoveQuestItem(s, QUEST_SONG_TIME);
            break;
        case RI_REMAINS_GOHT:
            RemoveQuestItem(s, QUEST_REMAINS_GOHT);
            break;
        case RI_REMAINS_GYORG:
            RemoveQuestItem(s, QUEST_REMAINS_GYORG);
            break;
        case RI_REMAINS_ODOLWA:
            RemoveQuestItem(s, QUEST_REMAINS_ODOLWA);
            break;
        case RI_REMAINS_TWINMOLD:
            RemoveQuestItem(s, QUEST_REMAINS_TWINMOLD);
            break;
        LS_SOUL_CASES:
            ClearRandoInf(s, SOUL_RI_TO_RANDO_INF(randoItemId));
            break;
        case RI_FROG_BLUE:
            ClearWeekEventReg(s, WEEKEVENTREG_33_01);
            break;
        case RI_FROG_CYAN:
            ClearWeekEventReg(s, WEEKEVENTREG_32_40);
            break;
        case RI_FROG_PINK:
            ClearWeekEventReg(s, WEEKEVENTREG_32_80);
            break;
        case RI_FROG_WHITE:
            ClearWeekEventReg(s, WEEKEVENTREG_33_02);
            break;
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_UP:
            ClearRandoInf(s, RANDO_INF_OBTAINED_OCARINA_BUTTON_A + (randoItemId - RI_OCARINA_BUTTON_A));
            break;
        case RI_BOMBCHU:
        case RI_DEKU_STICK:
        case RI_DEKU_NUT:
        case RI_MILK_REFILL:
        case RI_RED_POTION_REFILL:
        case RI_GREEN_POTION_REFILL:
        case RI_BLUE_POTION_REFILL:
        case RI_FAIRY_REFILL:
        case RI_GOLD_DUST_REFILL:
            break;
        default:
            if (Rando::StaticData::Items[randoItemId].itemId < 77) {
                DeleteItem(s, SLOT(Rando::StaticData::Items[randoItemId].itemId));
            }
            break;
    }
}

}

}
