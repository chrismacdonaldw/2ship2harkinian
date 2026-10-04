#ifndef RANDO_CHECK_DELIVERY_H
#define RANDO_CHECK_DELIVERY_H

#include <cstdint>
#include <string>
#include "Types.h"

struct PlayState;
namespace Notification {
struct Options;
}

namespace Rando::CheckDelivery {

enum class ClaimDisposition { Queue, Deferred, InboxOwned };
enum class GrantResult { Applied, AlreadyOwned, Deferred };
enum class JournalClaimKind { Local, Held, Accepted };
enum class InboxKind { Plain, Notify, Own };

struct Owner {
    uint64_t generation = 0;
    bool active = false;
    bool admitted = false;
};
struct JournalClaim {
    JournalClaimKind kind = JournalClaimKind::Held;
    int inboxIndex = -1;
};
struct InboxEntry {
    InboxKind kind = InboxKind::Plain;
    std::string itemName;
    std::string sourceText;
};
struct Ticket {
    uint64_t epoch = 0;
    ClaimDisposition disposition = ClaimDisposition::Deferred;
};
struct GrantOutcome {
    GrantResult result = GrantResult::Deferred;
    RandoItemId item = RI_UNKNOWN;
    std::string trapMessage;
};

// Callbacks run synchronously on the game thread and must not pump or replace the save owner.
// An inactive binding defers; active, unadmitted owners retain ordinary local randomizer behavior.
struct Binding {
    Owner (*readOwner)() = nullptr;
    JournalClaim (*claim)(RandoCheckId) = nullptr;
    int (*inboxCount)() = nullptr;
    bool (*readInbox)(int, InboxEntry&) = nullptr;
    bool (*canDeliver)() = nullptr;
    void (*decorateNotification)(int, Notification::Options&) = nullptr;
};

void SetBinding(const Binding* binding);
void Reset();
// Curiosity Shop keeps its delayed offer reservation through a same-owner park.
Ticket ClaimCheck(RandoCheckId check, bool delayedOffer = false);
// Shops prepare immediately before returning a successful canBuy; buy consumes the prepared data.
// Preparation stages at most 64 predecessor entries. A larger backlog drains through DeliverInbox.
bool PrepareCheck(Ticket ticket, RandoCheckId check);
void CancelCheck(Ticket ticket, RandoCheckId check);
// Resolves the raw placement after predecessor effects. shopPrice >= 0 commits payment with the grant.
// Deferred/AlreadyOwned never charge. Applied completes the native check and returns its resolved item.
GrantOutcome GrantCheck(Ticket ticket, RandoCheckId check, int shopPrice = -1);
bool DeliverInbox(PlayState* play);
// Existing native receipt storage; journal restoration and its durable meaning belong to the caller.
uint16_t& Received();
bool ApplyCollected(RandoCheckId check);
void RestoreCycleChests(PlayState* play);
RandoItemId ResolvePending(RandoItemId item, int skipIndex);

} // namespace Rando::CheckDelivery

#endif
