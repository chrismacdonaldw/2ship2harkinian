#include "CheckDelivery.h"
#include "Rando.h"
#include "Logic/LogicState.h"
#include "MiscBehavior/Traps.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include <limits>
#include <map>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

namespace Rando::CheckDelivery {
namespace {
constexpr size_t PREPARED_ENTRY_LIMIT = 64;
struct PreparedEntry {
    InboxEntry entry;
    RandoItemId item = RI_UNKNOWN;
};
struct Reservation {
    Ticket ticket;
    uint64_t ownerEpoch = 0;
    int inboxIndex = -1;
    int preparedFrom = -1;
    bool prepared = false;
    bool delayedOffer = false;
    std::vector<PreparedEntry> predecessors;
};
Binding sBinding;
bool sBound = false;
Owner sOwner;
uint64_t sOwnerEpoch = 1;
uint64_t sNextTicket = 0;
std::map<RandoCheckId, Reservation> sReservations;

bool ValidCheck(RandoCheckId check) {
    return check > RC_UNKNOWN && check < RC_MAX && StaticData::Checks.contains(check);
}

Owner SyncOwner() {
    const Owner owner = sBound ? (sBinding.readOwner != nullptr ? sBinding.readOwner() : Owner{})
                               : Owner{ 0, IS_RANDO && gSaveContext.fileNum != 0xFF, false };
    if (owner.generation != sOwner.generation) {
        Reset();
    } else if (owner.active != sOwner.active || owner.admitted != sOwner.admitted) {
        ++sOwnerEpoch;
        for (auto it = sReservations.begin(); it != sReservations.end();) {
            // Curiosity Shop removes stock before its delayed offer, which may survive a park.
            if (!it->second.delayedOffer) {
                it = sReservations.erase(it);
            } else {
                it->second.ownerEpoch = sOwnerEpoch;
                it->second.prepared = false;
                it->second.predecessors.clear();
                ++it;
            }
        }
    }
    sOwner = owner;
    return owner;
}

Reservation* FindReservation(Ticket ticket, RandoCheckId check) {
    if (!SyncOwner().active || !IS_RANDO || !ValidCheck(check))
        return nullptr;
    auto it = sReservations.find(check);
    if (it == sReservations.end() || it->second.ownerEpoch != sOwnerEpoch || it->second.ticket.epoch != ticket.epoch ||
        it->second.ticket.disposition != ticket.disposition)
        return nullptr;
    return &it->second;
}

bool ValidEntry(const InboxEntry& entry, RandoItemId& item) {
    if (entry.itemName.empty() || entry.itemName.size() >= 160 || entry.sourceText.size() >= 160 ||
        entry.itemName.find('\0') != std::string::npos || entry.sourceText.find('\0') != std::string::npos ||
        (entry.kind != InboxKind::Plain && entry.kind != InboxKind::Notify && entry.kind != InboxKind::Own))
        return false;
    const char* name = entry.itemName.c_str();
    if (entry.itemName.starts_with("mm:"))
        name += 3;
    if (*name == '\0')
        return false;
    item = StaticData::GetItemIdFromName(name);
#ifdef DIPTYCH_GAME_MODULE
    if (item == RI_DIPTYCH_FOREIGN)
        return false;
#endif
    // Unknown legacy names were consumed without a native effect; keep that receipt behavior explicit.
    return item == RI_UNKNOWN || StaticData::Items.contains(item);
}

bool ReadEntry(int index, PreparedEntry& out) {
    return index >= 0 && index < std::numeric_limits<uint16_t>::max() && sBinding.readInbox != nullptr &&
           sBinding.readInbox(index, out.entry) && ValidEntry(out.entry, out.item);
}

bool Ready(PlayState* play) {
    if (play == nullptr || play != gPlayState || !play->state.running || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->transitionMode != TRANS_MODE_OFF || IS_PAUSED(&play->pauseCtx))
        return false;
    Player* player = GET_PLAYER(play);
    return player != nullptr && !(player->stateFlags1 & PLAYER_STATE1_DEAD) &&
           (!sBound || sBinding.canDeliver == nullptr || sBinding.canDeliver());
}

void ConsumeEntry(int index, const PreparedEntry& prepared) {
    if (prepared.item == RI_UNKNOWN) {
        Received() = static_cast<uint16_t>(index + 1);
        return;
    }
    RandoItemId item = ConvertItem(prepared.item);
    if (item == RI_JUNK)
        item = CurrentJunkItem();
    const std::string trap = item == RI_TRAP ? CustomMessage::RemoveColorCodes(GetTrapMessage()) : "";
    GiveItem(item);
    Received() = static_cast<uint16_t>(index + 1);
    const auto& entry = prepared.entry;
    if (entry.kind != InboxKind::Plain || !trap.empty()) {
        Notification::Options notification{
            .itemIcon = StaticData::GetIconTexturePath(item),
            .prefix = entry.kind == InboxKind::Notify ? entry.sourceText
                      : entry.kind == InboxKind::Own  ? "You found"
                                                      : "",
            .message = entry.kind == InboxKind::Notify ? "found" : "",
            .suffix = trap.empty() ? StaticData::GetItemName(item, false) : trap,
        };
        if (entry.kind == InboxKind::Notify && sBinding.decorateNotification != nullptr)
            sBinding.decorateNotification(index, notification);
        Notification::Emit(notification);
    }
}

void CompleteCheck(RandoCheckId check) {
    auto& state = RANDO_SAVE_CHECKS[check];
    state.obtained = true;
    state.cycleObtained = true;
    state.eligible = false;
}
} // namespace

void SetBinding(const Binding* binding) {
    Reset();
    sBinding = binding != nullptr ? *binding : Binding{};
    sBound = binding != nullptr;
    sOwner = {};
}

void Reset() {
    ++sOwnerEpoch;
    sReservations.clear();
}

uint16_t& Received() {
    return RANDO_SAVE_CHECKS[RC_UNKNOWN].price;
}

Ticket ClaimCheck(RandoCheckId check, bool delayedOffer) {
    const Owner owner = SyncOwner();
    if (!owner.active || !IS_RANDO || !ValidCheck(check))
        return {};
    sReservations.erase(check);
    Reservation reservation;
    reservation.ticket = { ++sNextTicket, ClaimDisposition::Queue };
    reservation.ownerEpoch = sOwnerEpoch;
    reservation.delayedOffer = delayedOffer;
    if (owner.admitted && !RANDO_SAVE_CHECKS[check].obtained) {
        if (sBinding.claim == nullptr)
            return {};
        const uint64_t ownerEpoch = sOwnerEpoch;
        const JournalClaim claimed = sBinding.claim(check);
        SyncOwner();
        if (ownerEpoch != sOwnerEpoch || claimed.kind == JournalClaimKind::Held)
            return {};
        if (claimed.kind == JournalClaimKind::Accepted && claimed.inboxIndex >= 0) {
            if (claimed.inboxIndex >= std::numeric_limits<uint16_t>::max())
                return {};
            PreparedEntry own;
            if (claimed.inboxIndex < Received()) {
                reservation.ticket.disposition = ClaimDisposition::InboxOwned;
            } else if (!ReadEntry(claimed.inboxIndex, own)) {
                return {};
            } else if (own.entry.kind != InboxKind::Own) {
                reservation.ticket.disposition = ClaimDisposition::InboxOwned;
            } else {
                reservation.inboxIndex = claimed.inboxIndex;
            }
            SyncOwner();
            if (ownerEpoch != sOwnerEpoch)
                return {};
        } else if (claimed.kind != JournalClaimKind::Accepted && claimed.kind != JournalClaimKind::Local) {
            return {};
        }
    }
    const Ticket ticket = reservation.ticket;
    sReservations.emplace(check, std::move(reservation));
    return ticket;
}

bool PrepareCheck(Ticket ticket, RandoCheckId check) {
    Reservation* reservation = FindReservation(ticket, check);
    if (reservation == nullptr || ticket.disposition != ClaimDisposition::Queue)
        return false;
    if (!Ready(gPlayState) || !StaticData::Items.contains(RANDO_SAVE_CHECKS[check].randoItemId))
        return false;
    reservation->prepared = false;
    reservation->predecessors.clear();
    const int received = Received();
    const int target = reservation->inboxIndex;
    if (target >= 0) {
        const int count = sBinding.inboxCount != nullptr ? sBinding.inboxCount() : -1;
        if (count < 0 || count > std::numeric_limits<uint16_t>::max() || target >= count)
            return false;
        if (target < received)
            return false;
        if (target - received > PREPARED_ENTRY_LIMIT)
            return false;
        PreparedEntry own;
        if (!ReadEntry(target, own) || own.entry.kind != InboxKind::Own)
            return false;
        std::vector<PreparedEntry> predecessors;
        for (int i = received; i < target; ++i) {
            PreparedEntry entry;
            if (!ReadEntry(i, entry))
                return false;
            predecessors.push_back(std::move(entry));
        }
        reservation = FindReservation(ticket, check);
        if (reservation == nullptr)
            return false;
        reservation->predecessors = std::move(predecessors);
    }
    reservation->preparedFrom = received;
    reservation->prepared = true;
    return true;
}

void CancelCheck(Ticket ticket, RandoCheckId check) {
    auto it = sReservations.find(check);
    if (it != sReservations.end() && it->second.ticket.epoch == ticket.epoch)
        sReservations.erase(it);
}

GrantOutcome GrantCheck(Ticket ticket, RandoCheckId check, int shopPrice) {
    const auto deferred = [&]() -> GrantOutcome {
        if (shopPrice >= 0) {
            // A failed delayed offer has no retry; let the admitted inbox recover its award.
            CancelCheck(ticket, check);
        }
        return {};
    };
    Reservation* reservation = FindReservation(ticket, check);
    if (reservation == nullptr)
        return deferred();
    if (ticket.disposition == ClaimDisposition::InboxOwned ||
        (reservation->inboxIndex >= 0 && reservation->inboxIndex < Received())) {
        CompleteCheck(check);
        sReservations.erase(check);
        return { GrantResult::AlreadyOwned };
    }
    if (ticket.disposition != ClaimDisposition::Queue || shopPrice < -1)
        return deferred();
    if (!reservation->prepared && (shopPrice >= 0 || !PrepareCheck(ticket, check)))
        return deferred();
    reservation = FindReservation(ticket, check);
    if (reservation == nullptr || !reservation->prepared || reservation->preparedFrom != Received())
        return deferred();
    if (!StaticData::Items.contains(RANDO_SAVE_CHECKS[check].randoItemId) ||
        (shopPrice >= 0 && gSaveContext.save.saveInfo.playerData.rupees < shopPrice))
        return deferred();
    // All fallible reads and receipt bounds are settled before any inventory or payment effect.
    const int target = reservation->inboxIndex;
    auto predecessors = std::move(reservation->predecessors);
    sReservations.erase(check);
    int index = Received();
    for (const auto& entry : predecessors)
        ConsumeEntry(index++, entry);
    RandoItemId item = ConvertItem(RANDO_SAVE_CHECKS[check].randoItemId, check);
    if (shopPrice >= 0)
        Rupees_ChangeBy(-shopPrice);
    if (item == RI_JUNK)
        item = CurrentJunkItem(check);
    std::string trapMessage;
    if (item == RI_TRAP) {
        if (shopPrice >= 0)
            RollTrapType();
        else
            trapMessage = GetTrapMessage();
    }
    if (shopPrice < 0 && item == RI_TRIFORCE_PIECE)
        item = RI_TRIFORCE_PIECE_PREVIOUS;
    GiveItem(item);
    if (target >= 0)
        Received() = static_cast<uint16_t>(target + 1);
    CompleteCheck(check);
    return { GrantResult::Applied, item, std::move(trapMessage) };
}

bool DeliverInbox(PlayState* play) {
    const Owner owner = SyncOwner();
    if (!sBound || !owner.active || !owner.admitted || !IS_RANDO || !Ready(play) || sBinding.inboxCount == nullptr)
        return false;
    const int received = Received();
    const int count = sBinding.inboxCount();
    if (count <= received || count > std::numeric_limits<uint16_t>::max())
        return false;
    for (const auto& [check, reservation] : sReservations) {
        if (reservation.inboxIndex == received)
            return false;
    }
    const uint64_t ownerEpoch = sOwnerEpoch;
    PreparedEntry entry;
    if (!ReadEntry(received, entry))
        return false;
    SyncOwner();
    if (ownerEpoch != sOwnerEpoch || Received() != received)
        return false;
    ConsumeEntry(received, entry);
    return true;
}

bool ApplyCollected(RandoCheckId check) {
    if (!SyncOwner().active || !IS_RANDO || !ValidCheck(check) || RANDO_SAVE_CHECKS[check].obtained)
        return false;
    CompleteCheck(check);
    return true;
}

void RestoreCycleChests(PlayState* play) {
    if (!SyncOwner().active || !IS_RANDO)
        return;
    for (const auto& [check, location] : StaticData::Checks) {
        const auto& state = RANDO_SAVE_CHECKS[check];
        // Claims survive Song of Time; chest appearance belongs to the current cycle.
        // Treasure Chest Game reuses these flags for different forms.
        if (!state.shuffled || !state.cycleObtained || location.flagType != FLAG_CYCL_SCENE_CHEST ||
            location.sceneId == SCENE_TAKARAYA || location.sceneId < 0 || location.sceneId >= SCENE_MAX ||
            location.flag < 0 || location.flag >= 32)
            continue;
        const auto scene = Play_GetOriginalSceneId(location.sceneId);
        const u32 bit = u32{ 1 } << location.flag;
        gSaveContext.cycleSceneFlags[scene].chest |= bit;
        if (play != nullptr && play == gPlayState && Play_GetOriginalSceneId(play->sceneId) == scene)
            play->actorCtx.sceneFlags.chest |= bit;
    }
}

RandoItemId ResolvePending(RandoItemId item, int skipIndex) {
    const Owner owner = SyncOwner();
    if (!owner.admitted || !IS_RANDO || sBinding.inboxCount == nullptr)
        return item;
    Logic::State state = Logic::FromSave(gSaveContext);
    const uint64_t ownerEpoch = sOwnerEpoch;
    const int count = sBinding.inboxCount();
    if (count < 0 || count > std::numeric_limits<uint16_t>::max())
        return item;
    for (int i = Received(); i < count; ++i) {
        PreparedEntry entry;
        if (i != skipIndex && ReadEntry(i, entry) && entry.item != RI_UNKNOWN)
            Logic::Apply(state, Logic::Resolve(state, entry.item));
    }
    SyncOwner();
    return ownerEpoch == sOwnerEpoch ? Logic::Resolve(state, item) : item;
}
} // namespace Rando::CheckDelivery
