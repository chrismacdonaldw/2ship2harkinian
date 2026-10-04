// Stubbed boundary services; the Python driver inserts current Anchor/Profile bodies verbatim.
#include <nlohmann/json.hpp>
#include <ship/utils/StrHash64.h>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
using s16 = int16_t;
constexpr int SCENE_MAX = 113, RC_MAX = 3, SAVETYPE_RANDO = 1;
struct Check { int randoItemId = 0; bool shuffled = false, eligible = false, cycleObtained = false, obtained = false, skipped = false; uint16_t price = 0; };
struct RandoInfo { uint32_t finalSeed = 42, randoSaveOptions[3] = {}; uint16_t randoStartingItems[4] = {}, sariaPriorityItems[4] = {}; Check randoSaveChecks[RC_MAX]; int randoInf = 0, randoEvents = 0, foundDungeonKeys = 0, foundTriforcePieces = 0, sariaHintsAvailable = 0; };
struct ShipSaveInfo { char commitHash[8] = "saved01"; int saveType = SAVETYPE_RANDO; uint64_t fileCreatedAt = 0; RandoInfo rando; };
struct { struct { ShipSaveInfo shipSaveInfo; } save; int inventory = 0, clock = 0; uint8_t fileNum = 0; } gSaveContext;
namespace AnchorProgress {
struct State { bool known = false; uint32_t switches[SCENE_MAX][2] = {}; };
struct Edit { int16_t scene; uint8_t flag; bool set; };
enum class ApplyResult { Rejected, NotReady, Unknown, Applied };
State native; bool captureReady = true, applyReady = true, loadedReady = true; int applied = 0;
int16_t CanonicalScene(int16_t scene) { return scene >= 0 && scene < SCENE_MAX ? scene : -1; }
uint32_t EligibleMask(int16_t scene, uint8_t bank) { return scene == 34 && bank == 0 ? 1u << 10 : 0; }
bool LocalReady() { return loadedReady; }
bool Capture(State& state) { if (!captureReady) return false; state = native; state.known = true; return true; }
ApplyResult Apply(const State& state) { if (!applyReady) return ApplyResult::NotReady; if (!state.known) return ApplyResult::Unknown; native = state; ++applied; return ApplyResult::Applied; }
void (*registeredEdit)(const Edit&) = nullptr;
void Register(void (*callback)(const Edit&)) { registeredEdit = callback; }
}
class Network {
public:
 std::atomic<bool> isEnabled{false}, isConnected{false}; std::atomic<uint64_t> connectionGeneration{1};
 std::vector<nlohmann::json> sent; std::queue<nlohmann::json> incoming;
 virtual ~Network() = default;
 bool Enable(const char*, uint16_t) { isEnabled = isConnected = true; return true; }
 void Disable() { isEnabled = isConnected = false; }
 bool QueueOutgoingPacket(nlohmann::json packet) { if (!isConnected) return false; sent.push_back(packet); return true; }
 void SwapIncomingPacketQueue(std::queue<nlohmann::json>& packets) { packets.swap(incoming); }
};
struct GameInteractor {
 struct OnGameStateMainStart {}; struct OnSaveLoad {}; struct OnPlayDestroy {};
 static GameInteractor* Instance;
 template<class Hook, class Callback> void RegisterGameHook(Callback) {}
} gi;
GameInteractor* GameInteractor::Instance = &gi;
const char* CVarGetString(const char* name, const char* fallback) { return std::string(name) == "gRemote.Anchor.RoomId" ? "room" : fallback; }
int CVarGetInteger(const char*, int fallback) { return fallback; }
template<class... T> void Log(T...) {}
#define SPDLOG_ERROR(...) Log(__VA_ARGS__)
PROFILE_BODY
#define private public
CLIENT_HEADER
#undef private
CLIENT_BODY
Anchor::Scope hostScope;
AnchorProgress::State hostState;
bool hostReady = true, writeOk = true, scopeAvailable = true; int commits = 0;
bool ScopeProvider(Anchor::Scope* scope) { *scope = hostScope; return scopeAvailable; }
bool Read(const Anchor::Scope*, AnchorProgress::State* state) { if (!hostReady) return false; *state = hostState; return true; }
bool Commit(const Anchor::Scope*, const AnchorProgress::State* state) { if (!writeOk) return false; hostState = *state; ++commits; return true; }
int main() {
 using namespace AnchorProgress;
 ShipSaveInfo profile;
 const auto seed = ProfileSeed(profile);
 assert(seed.size() == 29 && seed.starts_with("mm-profile-1:"));
 auto mutableCopy = profile;
 mutableCopy.fileCreatedAt = 123;
 auto& r = mutableCopy.rando;
 r.randoInf = r.randoEvents = r.foundDungeonKeys = r.foundTriforcePieces = r.sariaHintsAvailable = 1;
 r.randoSaveChecks[1].eligible = r.randoSaveChecks[1].cycleObtained = r.randoSaveChecks[1].obtained = r.randoSaveChecks[1].skipped = true;
 gSaveContext.inventory = gSaveContext.clock = 1;
 assert(ProfileSeed(mutableCopy) == seed);
 for (int field = 0; field < 7; ++field) {
  auto other = profile;
  if (field == 0) other.rando.randoSaveOptions[1] = 2;
  if (field == 1) other.rando.randoStartingItems[1] = 2;
  if (field == 2) other.rando.sariaPriorityItems[1] = 2;
  if (field == 3) other.rando.randoSaveChecks[1].randoItemId = 2;
  if (field == 4) other.rando.randoSaveChecks[1].shuffled = true;
  if (field == 5) other.rando.randoSaveChecks[1].price = 2;
  if (field == 6) other.commitHash[0] = 'x';
  assert(other.rando.finalSeed == profile.rando.finalSeed && ProfileSeed(other) != seed);
 }
 assert(ProfileSeed(profile) == seed);
 Anchor anchor;
 Anchor::Instance = &anchor;
#ifdef DIPTYCH_GAME_MODULE
 std::strcpy(hostScope.seed, "checked-combined"); hostScope.admitted = hostScope.mmActive = true; hostScope.ownerGeneration = 1;
 anchor.SetSessionCallbacks(ScopeProvider, nullptr, Commit);
 anchor.Pump(); assert(applied == 0 && !anchor.Connect());
 anchor.SetSessionCallbacks(ScopeProvider, Read, Commit);
 hostReady = false;
 anchor.Pump(); anchor.LocalEdit({34, 10, true}); anchor.LocalEdit({34, 10, false});
 assert(anchor.localEdits.size() == 2);
 anchor.Pump(); assert(applied == 0 && anchor.localEdits.size() == 2);
 anchor.SetSessionCallbacks(ScopeProvider, Read, Commit); assert(anchor.localEdits.size() == 2);
 assert(!anchor.FlushSessionEdits() && anchor.localEdits.size() == 2);
 scopeAvailable = false; anchor.Pump(); assert(anchor.localEdits.size() == 2);
 anchor.LocalEdit({34, 10, true}); anchor.LocalEdit({34, 10, false});
 assert(anchor.localEdits.size() == 4);
 ++gSaveContext.fileNum; anchor.LocalEdit({34, 10, true}); assert(anchor.localEdits.size() == 4); --gSaveContext.fileNum;
 ++anchor.nativeLoadGeneration; anchor.LocalEdit({34, 10, true}); assert(anchor.localEdits.size() == 4); --anchor.nativeLoadGeneration;
 ++gSaveContext.save.shipSaveInfo.rando.finalSeed; anchor.LocalEdit({34, 10, true}); assert(anchor.localEdits.size() == 4); --gSaveContext.save.shipSaveInfo.rando.finalSeed;
 scopeAvailable = true; hostScope.mmActive = false; loadedReady = false; anchor.Pump();
 assert(anchor.localEdits.size() == 4 && applied == 0); // Actual Suspend retains this instance.
 loadedReady = true; hostScope.mmActive = true;
 hostReady = true; hostState.known = true; writeOk = false;
 anchor.Pump(); assert(applied == 0 && anchor.localEdits.size() == 4);
 auto* closing = new Anchor(); Anchor::Instance = closing;
 closing->SetSessionCallbacks(ScopeProvider, Read, Commit); closing->LocalEdit({34, 10, true});
 AnchorProgress::Register([](const Edit&){});
 assert(!closing->FlushSessionEdits()); Anchor::Shutdown();
 assert(Anchor::Instance == nullptr && AnchorProgress::registeredEdit == nullptr);
 Anchor::Instance = &anchor;
 writeOk = true; applyReady = false;
 assert(anchor.FlushSessionEdits()); assert(commits == 4 && anchor.localEdits.empty() && applied == 0);
 assert(hostState.switches[34][0] == 0);
 applyReady = true; anchor.Pump(); assert(applied == 1 && native.switches[34][0] == 0 && anchor.sent.empty());
 hostScope.mmActive = false; hostState.switches[34][0] = 1u << 10;
 anchor.Pump(); assert(applied == 1);
 hostScope.mmActive = true; anchor.Pump(); assert(applied == 2 && native.switches[34][0] == (1u << 10));
 hostScope.ownerGeneration++; hostScope.seed[0] = 'n'; hostState = {};
 captureReady = false; anchor.Pump(); assert(applied == 2);
 anchor.SetSessionCallbacks(nullptr, nullptr, nullptr); anchor.Pump(); assert(applied == 2);
#else
 native.known = true;
 assert(anchor.Connect()); anchor.Pump();
 assert(anchor.sent.back()["type"] == "HANDSHAKE");
 anchor.Receive({{"type", "ALL_CLIENT_STATE"}, {"state", nlohmann::json::array({{{"self", true}, {"clientId", 7}}})}});
 anchor.Receive({{"type", "UPDATE_ROOM_STATE"}, {"state", {{"ownerClientId", 7}, {"syncItemsAndFlags", 1}}}});
 anchor.Pump(); assert(anchor.sent.back()["type"] == "DIPTYCH_METADATA_HELLO");
 anchor.Receive({{"type", "DIPTYCH_METADATA_HELLO"}, {"clientId", 0}, {"nonce", anchor.nonce}, {"epoch", "epoch"}, {"cap", 2}});
 assert(!anchor.authoritative && applied == 0);
 anchor.Receive({{"type", "DIPTYCH_METADATA_HELLO"}, {"clientId", 0}, {"nonce", anchor.nonce}, {"epoch", "epoch"}, {"cap", 3}});
 assert(anchor.request != 0 && anchor.sent.back()["mmSwitchOnly"] == true);
 auto baseline = anchor.Envelope("DIPTYCH_METADATA_REQUEST"); baseline["clientId"] = 0; baseline.erase("targetClientId");
 baseline.update({{"baseline", true}, {"mmSwitchOnly", true}, {"game", 1}, {"request", 99}});
 anchor.Receive(baseline);
 assert(anchor.sent.back()["known"] == nlohmann::json::array({false, false}) && anchor.sent.back()["mmSwitchKnown"] == true);
 auto page = anchor.Envelope("DIPTYCH_METADATA_STATE"); page["clientId"] = 0; page.erase("targetClientId");
 page.update({{"request", anchor.request}, {"seq", 1}, {"page", 0}, {"of", 2}, {"known", nlohmann::json::array({false, false})},
              {"checks", nlohmann::json::array()}, {"entrances", nlohmann::json::array()}, {"ootSwitches", nlohmann::json::array()},
              {"mmSwitchKnown", true}, {"mmSwitches", {{34, 1u << 10, 0}}}});
 auto wrong = page; wrong["clientId"] = 8; anchor.Receive(wrong); assert(anchor.stage.pages.empty());
 wrong = page; wrong["epoch"] = "wrong"; anchor.Receive(wrong); assert(anchor.stage.pages.empty());
 anchor.Receive(page); assert(!anchor.authoritative && applied == 0);
 page["page"] = 1; page["mmSwitches"] = nlohmann::json::array(); anchor.Receive(page); anchor.Pump();
 assert(anchor.authoritative && applied == 1 && native.switches[34][0] == (1u << 10));
 applyReady = false; captureReady = false;
 anchor.LocalEdit({34, 10, false}); anchor.LocalEdit({34, 10, true}); anchor.Pump();
 assert(anchor.sent.back()["mmSwitches"] == nlohmann::json::array({{34, 10, false}, {34, 10, true}}));
 assert(applied == 1);
 auto edit = anchor.Envelope("DIPTYCH_METADATA_EDIT"); edit["clientId"] = 0; edit.erase("targetClientId");
 edit.update({{"seq", 2}, {"checks", nlohmann::json::array()}, {"entrances", nlohmann::json::array()},
              {"ootSwitches", nlohmann::json::array()}, {"mmSwitches", {{34, 10, false}, {34, 10, true}}}});
 assert(anchor.awaitingLocalEcho == 2 && anchor.localEdits.size() == 2);
 edit["sourceClientId"] = 7;
 auto sent = anchor.sent.size(); anchor.Receive(edit); anchor.Pump(); assert(anchor.sequence == 2 && anchor.sent.size() == sent && applied == 1);
 assert(anchor.awaitingLocalEcho == 0 && anchor.localEdits.empty());
 applyReady = true; captureReady = true; anchor.Pump(); assert(applied == 2);
 edit["seq"] = 4; anchor.Receive(edit); anchor.Pump(); assert(!anchor.authoritative && anchor.request != 0 && applied == 2);
 page["request"] = anchor.request; page["seq"] = 5; page["page"] = 0; page["of"] = 1; page["mmSwitchKnown"] = false;
 anchor.Receive(page); anchor.Pump(); assert(applied == 2 && native.switches[34][0] == (1u << 10));
 anchor.LocalEdit({34, 10, false}); anchor.authoritative = true; anchor.request = 0; anchor.FlushLocal();
 assert(anchor.localEdits.size() == 1 && anchor.awaitingLocalEcho == 1);
 anchor.Disconnect(); assert(!anchor.isEnabled && anchor.localEdits.size() == 1 && anchor.awaitingLocalEcho == 0);
 assert(anchor.Connect()); anchor.Pump(); assert(anchor.localEdits.size() == 1);
 anchor.scope.ownerGeneration = 999; anchor.Pump(); assert(anchor.localEdits.empty()); // Unrelated native owner fences old rows.
 anchor.Disconnect();
#endif
 std::cout << "Native scope and Anchor boundary regression passed" << std::endl;
 Anchor::Instance = nullptr;
}
