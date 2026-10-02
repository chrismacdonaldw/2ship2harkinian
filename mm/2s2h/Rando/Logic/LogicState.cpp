#include "LogicState.h"
#include "LogicStateFields.h"

#include <cstring>
#include <sstream>

extern "C" {
extern SavePlayerData sSaveDefaultPlayerData;
extern ItemEquips sSaveDefaultItemEquips;
extern Inventory sSaveDefaultInventory;
}

namespace Rando {

namespace Logic {

thread_local State* tCurrentState = nullptr;

static_assert(sizeof(State::items) == sizeof(Inventory::items));
static_assert(sizeof(State::dungeonItems) == sizeof(Inventory::dungeonItems));
static_assert(sizeof(State::dungeonKeys) == sizeof(Inventory::dungeonKeys));
static_assert(sizeof(State::strayFairies) == sizeof(Inventory::strayFairies));
static_assert(sizeof(State::weekEventReg) == sizeof(SaveInfo::weekEventReg));
static_assert(sizeof(State::randoInf) == sizeof(RandoSaveInfo::randoInf));
static_assert(sizeof(State::events) == sizeof(RandoSaveInfo::randoEvents));
static_assert(sizeof(State::foundDungeonKeys) == sizeof(RandoSaveInfo::foundDungeonKeys));
static_assert(sizeof(State::options) == sizeof(RandoSaveInfo::randoSaveOptions));

void RefreshFromSave(State& state, const SaveContext& saveContext) {
    const SaveInfo& info = saveContext.save.saveInfo;
    const RandoSaveInfo& rando = saveContext.save.shipSaveInfo.rando;

    memcpy(state.items, info.inventory.items, sizeof(state.items));
    state.equipment = info.equips.equipment;
    state.upgrades = info.inventory.upgrades;
    state.questItems = info.inventory.questItems;
    memcpy(state.dungeonItems, info.inventory.dungeonItems, sizeof(state.dungeonItems));
    memcpy(state.dungeonKeys, info.inventory.dungeonKeys, sizeof(state.dungeonKeys));
    memcpy(state.strayFairies, info.inventory.strayFairies, sizeof(state.strayFairies));
    state.healthCapacity = info.playerData.healthCapacity;
    state.isMagicAcquired = info.playerData.isMagicAcquired;
    state.isDoubleMagicAcquired = info.playerData.isDoubleMagicAcquired;
    state.doubleDefense = info.playerData.doubleDefense;
    state.owlActivationFlags = info.playerData.owlActivationFlags;
    state.playerForm = saveContext.save.playerForm;
    state.skullTokenCount = info.skullTokenCount;
    state.stolenItems = info.stolenItems;
    memcpy(state.weekEventReg, info.weekEventReg, sizeof(state.weekEventReg));
    memcpy(state.randoInf, rando.randoInf, sizeof(state.randoInf));
    memcpy(state.events, rando.randoEvents, sizeof(state.events));
    memcpy(state.foundDungeonKeys, rando.foundDungeonKeys, sizeof(state.foundDungeonKeys));
    state.foundTriforcePieces = rando.foundTriforcePieces;
    state.sariaHintsAvailable = rando.sariaHintsAvailable;
    memcpy(state.options, rando.randoSaveOptions, sizeof(state.options));
    state.checks = rando.randoSaveChecks;
}

State FromSave(const SaveContext& saveContext) {
    State state{};
    RefreshFromSave(state, saveContext);
    state.regionTime = 0;
    return state;
}

State& LiveState() {
    // Gameplay can change the save between reads; refresh while retaining regionTime.
    static thread_local State live{};
    RefreshFromSave(live, gSaveContext);
    return live;
}

std::string Diff(const State& a, const State& b) {
    std::ostringstream out;
    auto field = [&](const char* name, const void* pa, const void* pb, size_t size, size_t elemSize) {
        if (memcmp(pa, pb, size) == 0) {
            return;
        }
        for (size_t i = 0; i < size / elemSize; i++) {
            const u8* ea = (const u8*)pa + i * elemSize;
            const u8* eb = (const u8*)pb + i * elemSize;
            if (memcmp(ea, eb, elemSize) != 0) {
                uint64_t va = 0, vb = 0;
                memcpy(&va, ea, elemSize);
                memcpy(&vb, eb, elemSize);
                out << name;
                if (size != elemSize) {
                    out << "[" << i << "]";
                }
                out << ": 0x" << std::hex << va << " vs 0x" << vb << std::dec << "; ";
            }
        }
    };
#define DIFF_FIELD(f) field(#f, &a.f, &b.f, sizeof(a.f), sizeof(a.f))
#define DIFF_ARRAY(f) field(#f, a.f, b.f, sizeof(a.f), sizeof(a.f[0]))
    DIFF_ARRAY(items);
    DIFF_FIELD(equipment);
    DIFF_FIELD(upgrades);
    DIFF_FIELD(questItems);
    DIFF_ARRAY(dungeonItems);
    DIFF_ARRAY(dungeonKeys);
    DIFF_ARRAY(strayFairies);
    DIFF_FIELD(healthCapacity);
    DIFF_FIELD(isMagicAcquired);
    DIFF_FIELD(isDoubleMagicAcquired);
    DIFF_FIELD(doubleDefense);
    DIFF_FIELD(owlActivationFlags);
    DIFF_FIELD(playerForm);
    DIFF_FIELD(skullTokenCount);
    DIFF_FIELD(stolenItems);
    DIFF_ARRAY(weekEventReg);
    DIFF_ARRAY(randoInf);
    DIFF_ARRAY(events);
    DIFF_ARRAY(foundDungeonKeys);
    DIFF_FIELD(foundTriforcePieces);
    DIFF_FIELD(sariaHintsAvailable);
    DIFF_ARRAY(options);
#undef DIFF_FIELD
#undef DIFF_ARRAY
    return out.str();
}

State NewFileState(const RandoSaveInfo& rando, const std::vector<RandoItemId>& startingItems) {
    State s{};
    memcpy(s.items, sSaveDefaultInventory.items, sizeof(s.items));
    s.equipment = sSaveDefaultItemEquips.equipment;
    s.upgrades = sSaveDefaultInventory.upgrades;
    s.questItems = sSaveDefaultInventory.questItems;
    memcpy(s.dungeonItems, sSaveDefaultInventory.dungeonItems, sizeof(s.dungeonItems));
    memcpy(s.dungeonKeys, sSaveDefaultInventory.dungeonKeys, sizeof(s.dungeonKeys));
    memcpy(s.strayFairies, sSaveDefaultInventory.strayFairies, sizeof(s.strayFairies));
    s.healthCapacity = sSaveDefaultPlayerData.healthCapacity;
    s.isMagicAcquired = sSaveDefaultPlayerData.isMagicAcquired;
    s.isDoubleMagicAcquired = sSaveDefaultPlayerData.isDoubleMagicAcquired;
    s.doubleDefense = sSaveDefaultPlayerData.doubleDefense;
    s.owlActivationFlags = sSaveDefaultPlayerData.owlActivationFlags;
    s.playerForm = PLAYER_FORM_HUMAN;

    memcpy(s.foundDungeonKeys, s.dungeonKeys, sizeof(s.foundDungeonKeys));
    SetWeekEventReg(s, WEEKEVENTREG_59_04);
    SetWeekEventReg(s, WEEKEVENTREG_31_04);
    SetEquipValue(s, EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    SetEquipValue(s, EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    memcpy(s.options, rando.randoSaveOptions, sizeof(s.options));
    s.checks = rando.randoSaveChecks;

    for (RandoItemId startingItem : startingItems) {
        Apply(s, Resolve(s, startingItem));
    }
    if (s.options[RO_STARTING_HEALTH] != 3) {
        s.healthCapacity = s.options[RO_STARTING_HEALTH] * 0x10;
    }
    if (s.options[RO_STARTING_CONSUMABLES]) {
        Apply(s, RI_DEKU_STICK);
        Apply(s, RI_DEKU_NUT);
    }
    return s;
}

}

}
