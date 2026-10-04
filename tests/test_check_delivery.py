"""Run the native check-delivery body with deterministic game and journal shells.

This checks ownership, receipt ordering and payment boundaries; it does not run game resources.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile

from test_save_write_results import function

ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using u32=uint32_t;
enum RandoCheckId { RC_UNKNOWN, RC_A, RC_B, RC_MAX };
enum RandoItemId { RI_UNKNOWN, RI_JUNK, RI_NONE, RI_PROGRESSIVE, RI_TIER1, RI_TIER2, RI_RUPEE,
                   RI_TRAP, RI_TRIFORCE_PIECE, RI_TRIFORCE_PIECE_PREVIOUS, RI_DIPTYCH_FOREIGN };
#include "CheckDelivery.h"
struct Actor { struct { struct { int x=0,z=RC_A; } rot; } home;
    Actor* parent=nullptr; void(*draw)()=nullptr; bool killed=false; };
struct Player { Actor actor; int stateFlags1=0,getItemId=1; } player;
#define CUSTOM_ITEM_FLAGS (actor->home.rot.x)
#define CUSTOM_ITEM_PARAM (actor->home.rot.z)
namespace CustomItem { constexpr int GIVE_ITEM_CUTSCENE=4; }
constexpr int GI_NONE=0,GI_SHIP=1;
void Actor_Kill(Actor* actor) { actor->killed=true; }

struct PlayState {
    struct { bool running=true; } state;
    struct { int unk1206C=10; } msgCtx;
    int transitionTrigger=0,transitionMode=0,sceneId=1;
    bool pauseCtx=false;
    Player* player=&::player;
    struct { struct { u32 chest=0; } sceneFlags; } actorCtx;
} play;
PlayState* gPlayState=&play;
struct Check { bool obtained=false,cycleObtained=false,eligible=false,shuffled=true; RandoItemId randoItemId=RI_PROGRESSIVE; uint16_t price=0; };
struct Save {
    int fileNum=0;
    struct { struct { struct { Check randoSaveChecks[RC_MAX]; } rando; } shipSaveInfo;
             struct { struct { int rupees=90; } playerData; } saveInfo; } save;
    struct { u32 chest=0; } cycleSceneFlags[4];
} gSaveContext;
#define RANDO_SAVE_CHECKS gSaveContext.save.shipSaveInfo.rando.randoSaveChecks
#define IS_RANDO true
#define IS_PAUSED(p) (*(p))
#define GET_PLAYER(p) ((p)->player)
constexpr int TRANS_TRIGGER_OFF=0,TRANS_MODE_OFF=0,PLAYER_STATE1_DEAD=1;
constexpr int FLAG_CYCL_SCENE_CHEST=1,SCENE_TAKARAYA=3,SCENE_MAX=4,SCENE_AYASHIISHOP=2;
int Play_GetOriginalSceneId(int scene) { return scene; }
int tier=0,roll=0,trapGiven=-1;
bool hostReady=true;
std::vector<RandoItemId> gives;
void Rupees_ChangeBy(int delta) { auto& r=gSaveContext.save.saveInfo.playerData.rupees; r=std::clamp(r+delta,0,99); }
int RollTrapType() { return ++roll; }
std::string GetTrapMessage() { RollTrapType(); return "trap"+std::to_string(roll); }
namespace CustomMessage { std::string RemoveColorCodes(std::string s) { return s; } }
namespace Notification {
struct Options { const char* itemIcon=nullptr; std::string prefix,message,suffix; };
std::vector<Options> notifications;
void Emit(Options n) { notifications.push_back(n); }
}
namespace Rando {
namespace StaticData {
struct Location { int flagType=FLAG_CYCL_SCENE_CHEST,sceneId=1,flag=2; };
std::map<RandoCheckId,Location> Checks{{RC_A,{}},{RC_B,{}}};
std::map<RandoItemId,int> Items{{RI_JUNK,0},{RI_NONE,0},{RI_PROGRESSIVE,0},{RI_TIER1,0},{RI_TIER2,0},
    {RI_RUPEE,0},{RI_TRAP,0},{RI_TRIFORCE_PIECE,0},{RI_TRIFORCE_PIECE_PREVIOUS,0},{RI_DIPTYCH_FOREIGN,0}};
RandoItemId GetItemIdFromName(const char* name) {
    const std::string s=name;
    if(s=="Progressive") return RI_PROGRESSIVE;
    if(s=="Rupee") return RI_RUPEE;
    if(s=="Trap") return RI_TRAP;
    if(s=="Junk") return RI_JUNK;
    if(s=="Foreign") return RI_DIPTYCH_FOREIGN;
    return RI_UNKNOWN;
}
const char* GetIconTexturePath(RandoItemId) { return "icon"; }
std::string GetItemName(RandoItemId item,bool) { return "item"+std::to_string(item); }
}
RandoItemId ConvertItem(RandoItemId item,RandoCheckId check=RC_UNKNOWN) {
    if(item==RI_PROGRESSIVE) return check!=RC_UNKNOWN && RANDO_SAVE_CHECKS[check].obtained ? RI_JUNK : tier==0 ? RI_TIER1 : RI_TIER2;
    return item;
}
RandoItemId CurrentJunkItem(RandoCheckId=RC_UNKNOWN) { return gSaveContext.save.saveInfo.playerData.rupees>=99 ? RI_NONE : RI_RUPEE; }
void GiveItem(RandoItemId item) {
    gives.push_back(item);
    if(item==RI_TIER1 || item==RI_TIER2) ++tier;
    if(item==RI_RUPEE) Rupees_ChangeBy(20);
    if(item==RI_TRAP) trapGiven=roll;
}
namespace Logic {
struct State { int tier; };
State FromSave(const Save&) { return {::tier}; }
RandoItemId Resolve(State state,RandoItemId item) { return item==RI_PROGRESSIVE ? state.tier==0 ? RI_TIER1 : RI_TIER2 : item; }
void Apply(State& state,RandoItemId item) { if(item==RI_TIER1 || item==RI_TIER2) ++state.tier; }
}
}
@NATIVE@
struct EnGirlA { struct { struct { struct { int z=RC_A; } rot; } world; } actor; } shopActor;
constexpr int CANBUY_RESULT_NEED_RUPEES=1,CANBUY_RESULT_CANNOT_GET_NOW=2,CANBUY_RESULT_SUCCESS_2=3;
bool CanBePurchased(Check,RandoCheckId) { return true; }
using RandoSaveCheck=Check;
using s32=int32_t;
@SHOP@
namespace CheckDelivery=Rando::CheckDelivery;
static bool queued=true;
static Rando::CheckDelivery::Ticket queuedTicket;
@QUEUE@
using namespace Rando::CheckDelivery;
Owner owner{1,true,true};
JournalClaim claim{JournalClaimKind::Accepted,0};
std::vector<InboxEntry> inbox;
int missing=-1,oversized=-1,reads=0,decorated=0;
bool changeOwnerOnRead=false;
Owner ReadOwner() { return owner; }
JournalClaim Claim(RandoCheckId) { return claim; }
int Count() { return oversized>=0 ? oversized : static_cast<int>(inbox.size()); }
bool Read(int index,InboxEntry& out) {
    ++reads;
    if(changeOwnerOnRead) { ++owner.generation; changeOwnerOnRead=false; }
    if(index==missing || index<0 || index>=inbox.size()) return false;
    out=inbox[index]; return true;
}
bool CanDeliver() { return hostReady; }
void Decorate(int,Notification::Options& n) { ++decorated; n.prefix="durable "+n.prefix; }
Binding binding{ReadOwner,Claim,Count,Read,CanDeliver,Decorate};
void Fresh() {
    ++owner.generation; owner.active=true; owner.admitted=true;
    gSaveContext=Save{}; play=PlayState{}; player={}; tier=0; roll=0; trapGiven=-1;
    hostReady=true; gives.clear(); inbox.clear(); Notification::notifications.clear();
    missing=-1; oversized=-1; reads=0; decorated=0; changeOwnerOnRead=false;
    claim={JournalClaimKind::Accepted,0}; SetBinding(&binding);
}
int checks=0;
void Check(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
int main() {
    Fresh();
    { Binding temporary=binding; SetBinding(&temporary); temporary={}; }
    inbox={{InboxKind::Own,"mm:Progressive","self"}};
    auto t=ClaimCheck(RC_A); Check(t.disposition==ClaimDisposition::Queue,"copied binding");
    Check(PrepareCheck(t,RC_A),"prepare owned");
    auto grant=GrantCheck(t,RC_A,10);
    Check(grant.result==GrantResult::Applied && grant.item==RI_TIER1 && Received()==1,"owned effect receipt");
    Check(RANDO_SAVE_CHECKS[RC_A].obtained && RANDO_SAVE_CHECKS[RC_A].cycleObtained && !RANDO_SAVE_CHECKS[RC_A].eligible,"native completion");
    Check(gSaveContext.save.saveInfo.playerData.rupees==80,"single payment");
    Check(GrantCheck(t,RC_A,10).result==GrantResult::Deferred && gives.size()==1,"consumed ticket");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""},{InboxKind::Own,"Progressive","self"}}; claim.inboxIndex=1;
    t=ClaimCheck(RC_A); Check(PrepareCheck(t,RC_A),"prepare predecessor"); reads=0;
    grant=GrantCheck(t,RC_A,10);
    Check(grant.item==RI_TIER2 && gives==std::vector<RandoItemId>{RI_TIER1,RI_TIER2} && Received()==2,"resolve after predecessor");
    Check(reads==0,"prepared grant no fallible reads");
    Fresh(); inbox={{InboxKind::Own,"Rupee","self"}}; RANDO_SAVE_CHECKS[RC_A].randoItemId=RI_RUPEE;
    t=ClaimCheck(RC_A); Check(PrepareCheck(t,RC_A),"prepare rupee");
    Check(GrantCheck(t,RC_A,10).result==GrantResult::Applied && gSaveContext.save.saveInfo.playerData.rupees==99,"pay before wallet grant");
    Fresh(); RANDO_SAVE_CHECKS[RC_A].obtained=true; gSaveContext.save.saveInfo.playerData.rupees=99;
    t=ClaimCheck(RC_A); Check(PrepareCheck(t,RC_A) && GrantCheck(t,RC_A,10).item==RI_RUPEE && gSaveContext.save.saveInfo.playerData.rupees==99,"repeat shop junk selected after payment");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""},{InboxKind::Own,"Progressive","self"}}; claim.inboxIndex=1; missing=0;
    t=ClaimCheck(RC_A); Check(!PrepareCheck(t,RC_A),"missing predecessor defers");
    Check(GrantCheck(t,RC_A).result==GrantResult::Deferred && Received()==0 && gives.empty() && !RANDO_SAVE_CHECKS[RC_A].obtained,"queue deferral no effects");
    missing=-1; Check(GrantCheck(t,RC_A).result==GrantResult::Applied,"queue retry succeeds");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A);
    Check(PrepareCheck(t,RC_A),"owner prep"); ++owner.generation;
    Check(GrantCheck(t,RC_A,10).result==GrantResult::Deferred && Received()==0 && gives.empty(),"replacement invalidates");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A);
    owner.active=false; Check(!DeliverInbox(&play) && GrantCheck(t,RC_A).result==GrantResult::Deferred,"inactive refuses");
    owner.active=true; Check(GrantCheck(t,RC_A).result==GrantResult::Deferred,"ordinary park invalidates");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A,true);
    owner.active=false; Check(!DeliverInbox(&play),"delayed parked no grant");
    owner.active=true; Check(!DeliverInbox(&play),"delayed owns exclusion after park");
    Check(PrepareCheck(t,RC_A) && GrantCheck(t,RC_A,10).result==GrantResult::Applied,"delayed fresh prepare");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""},{InboxKind::Own,"Progressive","self"}}; claim.inboxIndex=1;
    t=ClaimCheck(RC_A,true); Check(DeliverInbox(&play) && Received()==1,"predecessor may drain before delayed offer");
    Check(PrepareCheck(t,RC_A) && GrantCheck(t,RC_A,10).item==RI_TIER2,"delayed current cursor conversion");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A,true); missing=0;
    Check(!PrepareCheck(t,RC_A) && GrantCheck(t,RC_A,10).result==GrantResult::Deferred,"terminal delayed failure");
    missing=-1; Check(DeliverInbox(&play) && Received()==1 && gives.size()==1,"released delayed inbox recovery");
    Check(!RANDO_SAVE_CHECKS[RC_A].obtained && gSaveContext.save.saveInfo.playerData.rupees==90,"failure no completion payment");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A,true); Received()=1;
    Check(GrantCheck(t,RC_A,10).result==GrantResult::AlreadyOwned && gives.empty(),"covered receipt no duplicate");
    Check(RANDO_SAVE_CHECKS[RC_A].obtained && RANDO_SAVE_CHECKS[RC_A].cycleObtained && gSaveContext.save.saveInfo.playerData.rupees==90,"covered receipt accepted completion no payment");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A); auto other=ClaimCheck(RC_B);
    Check(GrantCheck(t,RC_B).result==GrantResult::Deferred && gives.empty(),"ticket tied to check");
    Reset(); Check(GrantCheck(t,RC_A).result==GrantResult::Deferred,"native load reset");
    Fresh(); claim={JournalClaimKind::Held,-1};
    Check(ClaimCheck(RC_A).disposition==ClaimDisposition::Deferred && gives.empty(),"durable hold");
    claim={JournalClaimKind::Accepted,-1}; RANDO_SAVE_CHECKS[RC_A].randoItemId=RI_DIPTYCH_FOREIGN;
    t=ClaimCheck(RC_A); grant=GrantCheck(t,RC_A);
    Check(grant.result==GrantResult::Applied && grant.item==RI_DIPTYCH_FOREIGN && Received()==0,"foreign routing completed native noop");
    Fresh(); owner.admitted=false; claim={JournalClaimKind::Held,-1};
    Check(GrantCheck(ClaimCheck(RC_A),RC_A).result==GrantResult::Applied && Received()==0,"unpaired native fallback");
    SetBinding(nullptr); Check(GrantCheck(ClaimCheck(RC_B),RC_B).result==GrantResult::Applied,"standalone native fallback");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""}};
    hostReady=false; Check(!DeliverInbox(&play),"host door veto"); hostReady=true;
    play.pauseCtx=true; Check(!DeliverInbox(&play),"pause veto"); play.pauseCtx=false;
    player.stateFlags1=PLAYER_STATE1_DEAD; Check(!DeliverInbox(&play),"dead veto"); player.stateFlags1=0;
    play.transitionMode=1; Check(!DeliverInbox(&play),"transition veto"); play.transitionMode=0;
    Check(DeliverInbox(&play) && !DeliverInbox(&play),"one receipt then drained");
    Fresh(); inbox={{InboxKind::Notify,"Trap","friend"}};
    Check(DeliverInbox(&play) && decorated==1 && Notification::notifications.back().prefix=="durable friend" && trapGiven==roll,"native trap and decorated notify");
    Fresh(); inbox={{InboxKind::Plain,"old unknown name",""}};
    Check(DeliverInbox(&play) && Received()==1 && gives.empty(),"legacy unknown receipt skip");
    Fresh(); inbox={{InboxKind::Own,std::string("Progressive\0junk",16),""}};
    Check(ClaimCheck(RC_A).disposition==ClaimDisposition::Deferred && !DeliverInbox(&play) && Received()==0,"malformed no effects");
    Fresh(); oversized=65536; inbox={{InboxKind::Plain,"Progressive",""}};
    Check(!DeliverInbox(&play) && gives.empty(),"count capacity fence");
    claim.inboxIndex=0; inbox[0].kind=InboxKind::Own; t=ClaimCheck(RC_A); Check(!PrepareCheck(t,RC_A) && gives.empty(),"owned count capacity preflight");
    claim.inboxIndex=65535; Check(ClaimCheck(RC_A).disposition==ClaimDisposition::Deferred,"index capacity fence");
    Fresh(); inbox.assign(66,{InboxKind::Plain,"Progressive",""}); inbox.back().kind=InboxKind::Own; claim.inboxIndex=65;
    t=ClaimCheck(RC_A); Check(!PrepareCheck(t,RC_A) && gives.empty(),"bounded preparation");
    Check(DeliverInbox(&play) && PrepareCheck(t,RC_A),"large backlog drains before preparation");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; t=ClaimCheck(RC_A); changeOwnerOnRead=true;
    Check(!PrepareCheck(t,RC_A) && gives.empty() && Received()==0,"owner changes during preflight");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""}}; owner.active=false;
    Check(ResolvePending(RI_PROGRESSIVE,-1)==RI_TIER2 && gives.empty() && Received()==0,"parked read-only pending preview");
    Check(ResolvePending(RI_PROGRESSIVE,0)==RI_TIER1,"finder own entry excluded");
    Fresh(); Check(ApplyCollected(RC_A) && !ApplyCollected(RC_A),"teammate check accepted once");
    RANDO_SAVE_CHECKS[RC_A].cycleObtained=false; Check(!ApplyCollected(RC_A) && !RANDO_SAVE_CHECKS[RC_A].cycleObtained,"replay preserves new-cycle distinction");
    RANDO_SAVE_CHECKS[RC_A].cycleObtained=true; RestoreCycleChests(&play);
    Check(gSaveContext.cycleSceneFlags[1].chest==4 && play.actorCtx.sceneFlags.chest==4,"cycle chest restoration");
    Fresh(); Rando::StaticData::Checks[RC_A].sceneId=SCENE_TAKARAYA; RANDO_SAVE_CHECKS[RC_A].cycleObtained=true; RestoreCycleChests(&play);
    Check(gSaveContext.cycleSceneFlags[SCENE_TAKARAYA].chest==0,"treasure chest mapping excluded");
    Fresh(); inbox={{InboxKind::Own,"Rupee","self"}}; RANDO_SAVE_CHECKS[RC_A].randoItemId=RI_RUPEE;
    Check(EnGirlA_RandoCanBuyFunc(&play,&shopActor)==CANBUY_RESULT_SUCCESS_2,"actual immediate canBuy prepares");
    reads=0; EnGirlA_RandoBuyFunc(&play,&shopActor);
    Check(reads==0 && Received()==1 && gSaveContext.save.saveInfo.playerData.rupees==99,"actual immediate buy atomic wallet order");
    Fresh(); inbox={{InboxKind::Plain,"Progressive",""},{InboxKind::Own,"Progressive","self"}}; claim.inboxIndex=1; missing=0;
    Check(EnGirlA_RandoCanBuyFunc(&play,&shopActor)==CANBUY_RESULT_CANNOT_GET_NOW && gives.empty(),"actual shop defers entire purchase");
    missing=-1; Check(DeliverInbox(&play) && DeliverInbox(&play) && Received()==2,"denied immediate purchase releases inbox exclusion");
    Fresh(); play.sceneId=SCENE_AYASHIISHOP; inbox={{InboxKind::Plain,"Progressive",""},{InboxKind::Own,"Progressive","self"}}; claim.inboxIndex=1;
    Check(EnGirlA_RandoCanBuyFunc(&play,&shopActor)==CANBUY_RESULT_SUCCESS_2 && gives.empty(),"actual curiosity keeps delayed timing");
    owner.active=false; Check(!DeliverInbox(&play),"actual curiosity parked"); owner.active=true;
    Check(DeliverInbox(&play),"actual curiosity predecessors may drain"); EnGirlA_RandoBuyFunc(&play,&shopActor);
    Check(Received()==2 && gives.back()==RI_TIER2 && gSaveContext.save.saveInfo.playerData.rupees==80,"actual curiosity fresh grant after park");
    Fresh(); play.sceneId=SCENE_AYASHIISHOP; inbox={{InboxKind::Own,"Progressive","self"}};
    Check(EnGirlA_RandoCanBuyFunc(&play,&shopActor)==CANBUY_RESULT_SUCCESS_2,"actual curiosity claim");
    missing=0; EnGirlA_RandoBuyFunc(&play,&shopActor); missing=-1;
    Check(DeliverInbox(&play) && gSaveContext.save.saveInfo.playerData.rupees==90,"actual curiosity terminal failure inbox recovery");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}};
    queuedTicket=ClaimCheck(RC_A); ++owner.generation;
    Actor actor; actor.parent=&player.actor; actor.home.rot.x=CustomItem::GIVE_ITEM_CUTSCENE; actor.draw=+[](){};
    player.getItemId=GI_SHIP; queued=true; QueueGivePrefix(&actor,&play);
    Check(!queued && actor.killed && actor.draw==nullptr && player.getItemId==GI_NONE,"actual deferred queue cancels owned get-item");
    Check(PlayerGetItemGuard(&play,&player),"native GI_NONE guard bypasses message load");
    Check(gives.empty() && Received()==0 && !RANDO_SAVE_CHECKS[RC_A].obtained,"canceled queue no stale success or award");
    Fresh(); inbox={{InboxKind::Own,"Progressive","self"}}; Received()=1; queuedTicket=ClaimCheck(RC_A);
    actor=Actor{}; actor.parent=&player.actor; actor.home.rot.x=CustomItem::GIVE_ITEM_CUTSCENE; player.getItemId=GI_SHIP;
    QueueGivePrefix(&actor,&play);
    Check(actor.killed && player.getItemId==GI_NONE && RANDO_SAVE_CHECKS[RC_A].obtained && gives.empty(),"actual covered queue cancels presentation and completes check");
    Fresh(); actor=Actor{}; actor.home.rot.x=CustomItem::GIVE_ITEM_CUTSCENE; player.getItemId=GI_SHIP;
    CancelQueuedGetItem(&actor,&play);
    Check(player.getItemId==GI_SHIP,"cleanup leaves unrelated player item alone");
    std::cout << checks << " native delivery boundary assertions passed\n";
}
'''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--emit', type=pathlib.Path)
    args = parser.parse_args()
    source = (ROOT / 'mm/2s2h/Rando/CheckDelivery.cpp').read_text()
    source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
    header = (ROOT / 'mm/2s2h/Rando/CheckDelivery.h').read_text().replace('#include "Types.h"', '')
    shop = 'mm/2s2h/Rando/ActorBehavior/EnGirlA.cpp'
    shop_body = "static std::map<RandoCheckId, Rando::CheckDelivery::Ticket> shopTickets;" + chr(10)
    shop_body += function(shop, 's32 EnGirlA_RandoCanBuyFunc(')
    shop_body += function(shop, 'void EnGirlA_RandoBuyFunc(')
    queue = (ROOT / 'mm/2s2h/Rando/MiscBehavior/CheckQueue.cpp').read_text()
    marker = '[](Actor* actor, PlayState* play) {'
    start = queue.index(marker) + len(marker)
    end = queue.index('const RandoItemId randoItemId = grant.item;', start)
    queue_body = function('mm/2s2h/Rando/MiscBehavior/CheckQueue.cpp', 'static void CancelQueuedGetItem(')
    queue_body += 'void QueueGivePrefix(Actor* actor, PlayState* play) {' + queue[start:end] + '}'
    player_source = (ROOT / 'mm/src/overlays/actors/ovl_player_actor/z_player.c').read_text()
    start = player_source.index('s32 func_808482E0(')
    start = player_source.index('{', start) + 1
    end = player_source.index('if (this->av1.actionVar1', start)
    guard = player_source[start:end].replace('this->', 'player->')
    queue_body += 'bool PlayerGetItemGuard(PlayState* play, Player* player) {' + guard + 'return false;}'
    fixture = FIXTURE.replace('@NATIVE@', source).replace('@SHOP@', shop_body).replace('@QUEUE@', queue_body)
    if args.emit:
        args.emit.mkdir(parents=True, exist_ok=True)
        (args.emit / 'CheckDelivery.h').write_text(header)
        (args.emit / 'fixture.cpp').write_text(fixture)
        return
    with tempfile.TemporaryDirectory(prefix='mm-check-delivery-') as directory:
        tmp = pathlib.Path(directory)
        (tmp / 'CheckDelivery.h').write_text(header)
        (tmp / 'fixture.cpp').write_text(fixture)
        cmd = [args.compiler, '-std=c++20', '-DDIPTYCH_GAME_MODULE', '-g', str(tmp / 'fixture.cpp'), '-o', str(tmp / 'fixture')]
        if args.sanitize:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(cmd, check=True)
        subprocess.run([str(tmp / 'fixture')], check=True)

if __name__ == '__main__':
    main()
