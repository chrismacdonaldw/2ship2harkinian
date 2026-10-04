#include "Anchor.h"
#ifdef ENABLE_ANCHOR
#include "Profile.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <chrono>
#include <cstring>
#include <sstream>
#include <spdlog/spdlog.h>

extern "C" {
#include "variables.h"
}

namespace {
using Json = nlohmann::json;
constexpr uint64_t MAX_SEQUENCE = (uint64_t(1) << 53) - 1;
constexpr size_t MAX_PENDING = 512, MAX_PAGES = 128, PAGE_ENTRIES = 64, MAX_BYTES = 512 * 1024;

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool Number(const Json& value, uint64_t max) {
    return value.is_number_integer() && (value.is_number_unsigned() || value.get<int64_t>() >= 0) &&
           value.get<uint64_t>() <= max;
}

bool Token(const Json& value, size_t max) {
    return value.is_string() && !value.get_ref<const std::string&>().empty() &&
           value.get_ref<const std::string&>().size() <= max &&
           value.get_ref<const std::string&>().find(char(0)) == std::string::npos;
}

bool SharingValue(const Json& state) {
    if (!state.is_object() || !state.contains("syncItemsAndFlags")) {
        return false;
    }
    const auto& on = state["syncItemsAndFlags"];
    return (on.is_boolean() && on.get<bool>()) || (Number(on, 1) && on.get<uint64_t>() == 1);
}

bool Arrays(const Json& packet) {
    size_t entries = 0;
    for (const char* name : { "checks", "entrances", "ootSwitches", "mmSwitches" }) {
        if (!packet.contains(name) || !packet[name].is_array()) {
            return false;
        }
        entries += packet[name].size();
    }
    return entries <= PAGE_ENTRIES;
}

bool SwitchRows(const Json& packet, AnchorProgress::State& state, bool baseline) {
    if (!Arrays(packet)) {
        return false;
    }
    if (baseline && (!packet.contains("mmSwitchKnown") || !packet["mmSwitchKnown"].is_boolean())) {
        return false;
    }
    if (!baseline && packet.contains("mmSwitchKnown")) {
        return false;
    }
    std::array<bool, SCENE_MAX> seen{};
    for (const auto& row : packet["mmSwitches"]) {
        if (!row.is_array() || row.size() != 3 || !Number(row[0], SCENE_MAX - 1)) {
            return false;
        }
        const auto scene = row[0].get<int16_t>();
        if (AnchorProgress::CanonicalScene(scene) != scene) {
            return false;
        }
        if (baseline) {
            if (!packet["mmSwitchKnown"].get<bool>() || seen[scene] || !Number(row[1], UINT32_MAX) ||
                !Number(row[2], UINT32_MAX)) {
                return false;
            }
            const uint32_t a = row[1].get<uint32_t>(), b = row[2].get<uint32_t>();
            if ((a | b) == 0 || (a & ~AnchorProgress::EligibleMask(scene, 0)) != 0 ||
                (b & ~AnchorProgress::EligibleMask(scene, 1)) != 0) {
                return false;
            }
            seen[scene] = true;
            state.switches[scene][0] = a;
            state.switches[scene][1] = b;
        } else {
            if (!Number(row[1], 63) || !row[2].is_boolean() || !state.known) {
                return false;
            }
            const auto flag = row[1].get<uint8_t>();
            const uint32_t bit = uint32_t(1) << (flag % 32);
            if ((AnchorProgress::EligibleMask(scene, flag / 32) & bit) == 0) {
                return false;
            }
            auto& bank = state.switches[scene][flag / 32];
            bank = row[2].get<bool>() ? bank | bit : bank & ~bit;
        }
    }
    return true;
}
bool OwlRows(const Json& packet, OwlAccess::State& state, bool baseline, unsigned capability) {
    if (capability < 4) {
        return !packet.contains("mmOwlKnown") && !packet.contains("mmOwls");
    }
    if (baseline) {
        if (!packet.contains("mmOwlKnown") || !packet["mmOwlKnown"].is_boolean() || !packet.contains("mmOwls") ||
            !Number(packet["mmOwls"], OwlAccess::MASK)) {
            return false;
        }
        state = { packet["mmOwlKnown"].get<bool>(), packet["mmOwls"].get<uint16_t>() };
        return state.known || state.mask == 0;
    }
    if (packet.contains("mmOwlKnown")) {
        return false;
    }
    if (packet.contains("mmOwls")) {
        if (!state.known || !Number(packet["mmOwls"], OwlAccess::MASK) || packet["mmOwls"] == 0) {
            return false;
        }
        state.mask |= packet["mmOwls"].get<uint16_t>();
    }
    return true;
}
} // namespace

Anchor* Anchor::Instance = nullptr;

void Anchor::Init() {
    if (Instance != nullptr) {
        return;
    }
    Instance = new Anchor();
    AnchorProgress::Register([](const AnchorProgress::Edit& edit) {
        if (Instance != nullptr) {
            try {
                Instance->LocalEdit(edit);
            } catch (const std::exception& error) {
                Instance->localQueueFailed = true;
                SPDLOG_ERROR("Anchor local progress retention failed: {}", error.what());
            }
        }
    });
    OwlAccess::Register([](uint16_t mask) {
        if (Instance != nullptr) {
            try {
                Instance->LocalOwls(mask);
            } catch (const std::exception& error) {
                Instance->localQueueFailed = true;
                SPDLOG_ERROR("Anchor local owl access retention failed: {}", error.what());
            }
        }
    });
    static bool hooksRegistered = false;
    if (hooksRegistered) {
        return;
    }
    hooksRegistered = true;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
        if (Instance != nullptr) {
            try {
                Instance->Pump();
            } catch (const std::exception& error) {
                SPDLOG_ERROR("Anchor progress rejected packet: {}", error.what());
                Instance->Disconnect();
            }
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        if (Instance != nullptr) {
            ++Instance->nativeLoadGeneration;
            Instance->nativeSeed.clear();
            Instance->refreshOwner = true;
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        if (Instance != nullptr) {
            Instance->refreshOwner = true;
        }
    });
}

void Anchor::Shutdown() {
    if (Instance != nullptr && !Instance->FlushSessionEdits()) {
        SPDLOG_ERROR("Anchor local progress remains unadmitted at final teardown; it was not persisted");
    }
    // Final teardown cannot retain callbacks or a worker attached to the context DeinitOTR destroys.
    // Ordinary module Suspend does not call this; its held edits remain available for orderly retry.
    AnchorProgress::Register(nullptr);
    OwlAccess::Register(nullptr);
    if (Instance != nullptr) {
        Instance->Disconnect();
        Instance->SetSessionCallbacks(nullptr, nullptr, nullptr);
        Instance->SetOwlCallbacks(nullptr, nullptr);
        delete Instance;
        Instance = nullptr;
    }
}

void Anchor::SetSessionCallbacks(ScopeProvider provider, ReadRetained read, CommitRetained commit) {
    if (scopeProvider == provider && readRetained == read && commitRetained == commit) {
        return;
    }
    scopeProvider = provider;
    readRetained = read;
    commitRetained = commit;
    scope = {};
    retained = {};
    retainedOwls = {};
    pendingOwls = 0;
    pendingOwlApply = false;
    localQueueFailed = false;
    localEdits.clear();
    ResetProtocol();
}

void Anchor::SetOwlCallbacks(ReadOwls read, CommitOwls commit) {
    if (readOwls == read && commitOwls == commit) {
        return;
    }
    readOwls = read;
    commitOwls = commit;
    retainedOwls = {};
    pendingOwls = 0;
    pendingOwlApply = false;
}

void Anchor::RememberNativeOwner(const Scope& checked) {
    if (!checked.mmActive || !AnchorProgress::LocalReady()) {
        return;
    }
    if (scopedLoadGeneration != nativeLoadGeneration || scopedFile != gSaveContext.fileNum ||
        scopedOwnerGeneration != checked.ownerGeneration || scopedProfile.empty()) {
        scopedProfile = AnchorProgress::ProfileSeed(gSaveContext.save.shipSaveInfo);
        scopedLoadGeneration = nativeLoadGeneration;
        scopedFile = gSaveContext.fileNum;
        scopedOwnerGeneration = checked.ownerGeneration;
    }
}

bool Anchor::SameNativeOwner() const {
    return scope.admitted && scope.mmActive && AnchorProgress::LocalReady() &&
           scopedLoadGeneration == nativeLoadGeneration && scopedFile == gSaveContext.fileNum &&
           scopedProfile == AnchorProgress::ProfileSeed(gSaveContext.save.shipSaveInfo);
}

bool Anchor::GetScope(Scope& next) {
    if (scopeProvider != nullptr) {
        if (readRetained == nullptr || commitRetained == nullptr || !scopeProvider(&next) || !next.admitted ||
            std::memchr(next.seed, 0, sizeof(next.seed)) == nullptr || next.seed[0] == 0) {
            return false;
        }
        RememberNativeOwner(next);
        return true;
    }
#ifdef DIPTYCH_GAME_MODULE
    return false; // A paired session cannot fall back to an unrelated standalone identity.
#else
    if (!AnchorProgress::LocalReady()) {
        return false;
    }
    if (nativeSeed.empty()) {
        nativeSeed = AnchorProgress::ProfileSeed(gSaveContext.save.shipSaveInfo);
    }
    std::memcpy(next.seed, nativeSeed.c_str(), nativeSeed.size() + 1);
    next.ownerGeneration = nativeLoadGeneration;
    next.admitted = true;
    next.mmActive = true;
    return true;
#endif
}

bool Anchor::Connect() {
#ifdef DIPTYCH_GAME_MODULE
    return false; // The embedding owner already owns the connection and ordered admission.
#else
    ResetProtocol();
    const std::string nextRoom = CVarGetString("gRemote.Anchor.RoomId", "");
    const std::string nextTeam = CVarGetString("gRemote.Anchor.TeamId", "default");
    if (room != nextRoom || team != nextTeam) {
        localEdits.clear();
        localQueueFailed = false;
        retained = {};
        retainedOwls = {};
        pendingOwls = 0;
        pendingOwlApply = false;
        pendingApply = false;
    }
    room = nextRoom;
    team = nextTeam;
    const int port = CVarGetInteger("gRemote.Anchor.Port", 43383);
    if (room.empty() || room.size() > 64 || team.empty() || team.size() > 64 || port < 1 || port > UINT16_MAX) {
        return false;
    }
    return Network::Enable(CVarGetString("gRemote.Anchor.Host", "127.0.0.1"), uint16_t(port));
#endif
}

void Anchor::Disconnect() {
    Network::Disable();
    ResetProtocol();
}

void Anchor::ResetProtocol() {
    clientId = ownerClientId = sequence = 0;
    capability = 0;
    sharing = authoritative = owlAuthoritative = false;
    epoch.clear();
    nonce.clear();
    stage = {};
    request = 0;
    awaitingLocalEcho = 0;
    incomingEdits.clear();
    lastHello = lastRequest = lastOwlSend = -10;
}

bool Anchor::Send(Json packet) {
    if (!isConnected) {
        return false;
    }
    packet["clientId"] = clientId;
    return QueueOutgoingPacket(std::move(packet));
}

Anchor::Json Anchor::Envelope(const char* type) const {
    return { { "type", type },
             { "targetClientId", 0 },
             { "epoch", epoch },
             { "diptych", { { "proto", 1 }, { "seed", scope.seed }, { "room", room }, { "team", team } } } };
}

Anchor::Json Anchor::ClientState() const {
    return { { "name", CVarGetString("gRemote.Anchor.Name", "") },
             { "clientVersion", "2S2H-permanent-progress-1" },
             { "teamId", team },
             { "online", true },
             { "isSaveLoaded", scope.admitted && scope.mmActive },
             { "diptych", { { "proto", 1 }, { "seed", scope.seed }, { "game", "mm" } } } };
}

void Anchor::Handshake() {
    Send({ { "type", "HANDSHAKE" },
           { "roomId", room },
           { "clientState", ClientState() },
           { "roomState",
             { { "ownerClientId", 0 },
               { "syncItemsAndFlags", CVarGetInteger("gRemote.Anchor.RoomSettings.SyncItemsAndFlags", 1) },
               { "pvpMode", 0 },
               { "showLocationsMode", 0 },
               { "teleportMode", 0 } } } });
}

void Anchor::RequestState() {
    if (!isConnected || clientId == 0 || capability < 3 || !sharing || !scope.admitted || epoch.empty()) {
        return;
    }
    authoritative = owlAuthoritative = false;
    awaitingLocalEcho = 0;
    stage = {};
    incomingEdits.clear();
    request = ++requestSerial;
    if (request == 0) {
        request = ++requestSerial;
    }
    auto packet = Envelope("DIPTYCH_METADATA_REQUEST");
    packet["request"] = request;
    if (scope.mmActive) {
        packet["game"] = 1;
        packet[capability >= 4 ? "mmProgressOnly" : "mmSwitchOnly"] = true;
    }
    lastRequest = Now();
    if (!Send(std::move(packet))) {
        request = 0;
    }
}

void Anchor::SendBaseline(uint32_t token) {
    AnchorProgress::State captured;
    if (!scope.mmActive || !AnchorProgress::Capture(captured)) {
        return; // The server's existing bounded nomination timeout retries when native capture is safe.
    }
    // Known retained admission plus pending native edits supplies a replacement baseline after server restart.
    if (retained.known && pendingApply) {
        captured = retained;
        for (const auto& edit : localEdits) {
            auto& bank = captured.switches[edit.scene][edit.flag / 32];
            const auto bit = uint32_t(1) << (edit.flag % 32);
            bank = edit.set ? bank | bit : bank & ~bit;
        }
    }
    OwlAccess::State owls;
    if (capability >= 4 && !OwlAccess::Capture(owls)) {
        return;
    }
    owls.mask |= retainedOwls.mask | pendingOwls;
    std::vector<Json> rows;
    for (int16_t scene = 0; scene < SCENE_MAX; ++scene) {
        if ((captured.switches[scene][0] | captured.switches[scene][1]) != 0) {
            rows.push_back(Json::array({ scene, captured.switches[scene][0], captured.switches[scene][1] }));
        }
    }
    const size_t pages = std::max(size_t(1), (rows.size() + PAGE_ENTRIES - 1) / PAGE_ENTRIES);
    for (size_t page = 0; page < pages; ++page) {
        auto packet = Envelope("DIPTYCH_METADATA_STATE");
        packet.update({ { "request", token },
                        { "seq", 0 },
                        { "page", page },
                        { "of", pages },
                        { "known", Json::array({ false, false }) },
                        { "checks", Json::array() },
                        { "entrances", Json::array() },
                        { "ootSwitchKnown", false },
                        { "ootSwitches", Json::array() },
                        { "mmSwitchKnown", true },
                        { "mmSwitches", Json::array() } });
        if (capability >= 4) {
            packet["mmOwlKnown"] = owls.known;
            packet["mmOwls"] = owls.mask;
        }
        for (size_t i = page * PAGE_ENTRIES; i < std::min(rows.size(), (page + 1) * PAGE_ENTRIES); ++i) {
            packet["mmSwitches"].push_back(rows[i]);
        }
        if (!Send(std::move(packet))) {
            return;
        }
    }
}

bool Anchor::Commit(const AnchorProgress::State& state) {
    if (commitRetained != nullptr && !commitRetained(&scope, &state)) {
        return false;
    }
    retained = state;
    pendingApply = state.known;
    return true;
}

bool Anchor::FinishState() {
    if (stage.pages.empty()) {
        return false;
    }
    OwlAccess::State owls = stage.owls;
    AnchorProgress::State candidate;
    candidate.known = stage.known;
    std::array<bool, SCENE_MAX> seen{};
    for (const auto& page : stage.pages) {
        if (page.is_null() || !SwitchRows(page, candidate, true)) {
            return false;
        }
        for (const auto& row : page["mmSwitches"]) {
            const auto scene = row[0].get<int16_t>();
            if (seen[scene]) {
                RequestState();
                return false;
            }
            seen[scene] = true;
        }
    }
    if (!CommitOwlState(owls)) {
        return false;
    }
    owlAuthoritative = owls.known;
    if (!candidate.known) {
        sequence = stage.sequence;
        request = 0;
        stage = {};
        authoritative = false;
        return true; // UNKNOWN never clears retained or native state.
    }
    if (!Commit(candidate)) {
        return false;
    }
    sequence = stage.sequence;
    authoritative = true;
    request = 0;
    stage = {};
    return true;
}

void Anchor::Receive(const Json& packet) {
    if (!packet.is_object() || !packet.contains("type") || !packet["type"].is_string()) {
        return;
    }
    const std::string type = packet["type"].get<std::string>();
    if (type == "ALL_CLIENT_STATE" && (!packet.contains("clientId") || packet["clientId"] == 0) &&
        packet.contains("state") && packet["state"].is_array()) {
        for (const auto& client : packet["state"]) {
            if (client.is_object() && client.value("self", false) && client.contains("clientId") &&
                Number(client["clientId"], UINT32_MAX) && client["clientId"].get<uint64_t>() != 0) {
                clientId = client["clientId"].get<uint64_t>();
            }
        }
        return;
    }
    if (type == "UPDATE_ROOM_STATE" && packet.contains("state") && packet["state"].is_object() &&
        packet["state"].contains("ownerClientId") && Number(packet["state"]["ownerClientId"], UINT32_MAX)) {
        const bool wasOn = sharing;
        ownerClientId = packet["state"]["ownerClientId"].get<uint64_t>();
        sharing = room != "soh-global" && SharingValue(packet["state"]);
        if (sharing != wasOn) {
            authoritative = owlAuthoritative = false;
            stage = {};
            incomingEdits.clear();
            request = 0;
            awaitingLocalEcho = 0; // Previously admitted local events survive; new OFF events are not queued.
            if (sharing) {
                RequestState();
            }
        }
        return;
    }
    if (!packet.contains("clientId") || !Number(packet["clientId"], 0) ||
        (packet.contains("targetClientId") &&
         (!Number(packet["targetClientId"], UINT32_MAX) || packet["targetClientId"] != clientId))) {
        return;
    }
    if (type == "DIPTYCH_METADATA_HELLO") {
        if (packet.contains("nonce") && packet["nonce"] == nonce && packet.contains("epoch") &&
            Token(packet["epoch"], 32) && packet.contains("cap") && Number(packet["cap"], 4)) {
            capability = packet["cap"].get<unsigned>();
            epoch = packet["epoch"].get<std::string>();
            if (capability >= 3) {
                RequestState();
            }
        }
        return;
    }
    if (capability < 3 || !sharing || !scope.admitted || !packet.contains("epoch") || packet["epoch"] != epoch ||
        !packet.contains("diptych") || packet["diptych"] != Envelope(type.c_str())["diptych"]) {
        return;
    }
    if (type == "DIPTYCH_METADATA_REQUEST") {
        if (scope.mmActive && packet.value("baseline", false) &&
            packet.value(capability >= 4 ? "mmProgressOnly" : "mmSwitchOnly", false) && packet.contains("game") &&
            packet["game"] == 1 && packet.contains("request") && Number(packet["request"], UINT32_MAX) &&
            packet["request"].get<uint32_t>() != 0) {
            SendBaseline(packet["request"].get<uint32_t>());
        }
        return;
    }
    if (!packet.contains("seq") || !Number(packet["seq"], MAX_SEQUENCE)) {
        return;
    }
    if (type == "DIPTYCH_METADATA_STATE") {
        AnchorProgress::State checked;
        OwlAccess::State checkedOwls;
        if (request == 0 || !packet.contains("request") || packet["request"] != request || packet["seq"] == 0 ||
            !packet.contains("of") || !Number(packet["of"], MAX_PAGES) || packet["of"] == 0 ||
            !packet.contains("page") || !Number(packet["page"], MAX_PAGES - 1) || packet["page"] >= packet["of"] ||
            !SwitchRows(packet, checked, true) || !OwlRows(packet, checkedOwls, true, capability)) {
            return;
        }
        if (stage.pages.empty()) {
            stage.sequence = packet["seq"].get<uint64_t>();
            stage.known = packet["mmSwitchKnown"].get<bool>();
            stage.owls = checkedOwls;
            stage.pages.resize(packet["of"].get<size_t>());
        }
        if (stage.sequence != packet["seq"] || stage.known != packet["mmSwitchKnown"] ||
            stage.pages.size() != packet["of"].get<size_t>() || stage.owls.known != checkedOwls.known ||
            stage.owls.mask != checkedOwls.mask) {
            RequestState();
            return;
        }
        auto& page = stage.pages[packet["page"].get<size_t>()];
        if (!page.is_null()) {
            if (page != packet) {
                RequestState();
            }
            return;
        }
        const size_t bytes = packet.dump().size();
        if (stage.bytes + bytes > MAX_BYTES) {
            RequestState();
            return;
        }
        stage.bytes += bytes;
        page = packet;
        FinishState();
    } else if (type == "DIPTYCH_METADATA_EDIT") {
        if (packet.value("resync", false)) {
            request = 0;
            stage = {};
            authoritative = false;
            incomingEdits.clear();
            lastRequest = Now(); // Explicit server capacity rejection waits before retrying.
            return;
        }
        if (incomingEdits.size() == MAX_PENDING) {
            RequestState();
            return;
        }
        incomingEdits.push_back(packet);
    }
}

void Anchor::LocalEdit(const AnchorProgress::Edit& edit) {
    Scope current;
    if (!GetScope(current)) {
#ifdef DIPTYCH_GAME_MODULE
        // A temporary host barrier must not drop native events belonging to the last checked owner.
        // The native load generation, slot and immutable profile fence replacement saves.
        if (!SameNativeOwner()) {
            return;
        }
        current = scope;
#else
        return;
#endif
    }
    if (!current.mmActive) {
        return;
    }
    if (std::strcmp(current.seed, scope.seed) != 0 || current.ownerGeneration != scope.ownerGeneration) {
        ResetProtocol();
        scope = current;
        retained = {};
        retainedOwls = {};
        pendingOwls = 0;
        pendingOwlApply = false;
        pendingApply = false;
        localEdits.clear();
        localQueueFailed = false;
    }
#ifndef DIPTYCH_GAME_MODULE
    if (!retained.known && !AnchorProgress::Capture(retained)) {
        return;
    }
    auto& bank = retained.switches[edit.scene][edit.flag / 32];
    const uint32_t bit = uint32_t(1) << (edit.flag % 32);
    bank = edit.set ? bank | bit : bank & ~bit;
    // OFF/disconnected changes remain native-local and can seed a genuinely fresh namespace.
    if (!isConnected || !sharing || capability < 3) {
        return;
    }
#endif
    if (localEdits.size() == MAX_PENDING) {
        // Keep native state intact and stop claiming that the connection preserves every ordered change.
        localQueueFailed = true;
        SPDLOG_ERROR("Anchor local progress queue is full; sharing stopped with native state retained");
        Network::Disable();
        return;
    }
    localEdits.push_back(edit);
}

void Anchor::FlushLocal() {
    if (!authoritative || request != 0 || awaitingLocalEcho != 0 || localEdits.empty() || !scope.mmActive) {
        return;
    }
    auto packet = Envelope("DIPTYCH_METADATA_EDIT");
    packet.update({ { "checks", Json::array() },
                    { "entrances", Json::array() },
                    { "ootSwitches", Json::array() },
                    { "mmSwitches", Json::array() } });
    AnchorProgress::State candidate = retained;
    const size_t count = std::min(PAGE_ENTRIES, localEdits.size());
    for (size_t i = 0; i < count; ++i) {
        const auto& edit = localEdits[i];
        packet["mmSwitches"].push_back({ edit.scene, edit.flag, edit.set });
        auto& bank = candidate.switches[edit.scene][edit.flag / 32];
        const auto bit = uint32_t(1) << (edit.flag % 32);
        bank = edit.set ? bank | bit : bank & ~bit;
    }
    if (!Commit(candidate) || !Send(std::move(packet))) {
        return;
    }
    // Transport queue acceptance is not server admission. Retain the batch until its own ordered echo.
    awaitingLocalEcho = count;
    lastLocalSend = Now();
}

void Anchor::Pump() {
#ifdef DIPTYCH_GAME_MODULE
    Scope next;
    if (!GetScope(next)) {
        return;
    }
    if (next.ownerGeneration != scope.ownerGeneration || std::strcmp(next.seed, scope.seed) != 0) {
        scope = next;
        retained = {};
        retainedOwls = {};
        pendingOwls = 0;
        pendingOwlApply = false;
        pendingApply = false;
        localEdits.clear();
        localQueueFailed = false;
    }
    scope = next;
    if (localQueueFailed)
        return; // Do not overwrite native progress after a retention failure.
    PumpOwls();
    AnchorProgress::State admitted;
    if (!readRetained(&scope, &admitted)) {
        return; // Host baseline/I/O barrier: do not overwrite held local events or touch native state.
    }
    if (!admitted.known) {
        if (!scope.mmActive || !AnchorProgress::Capture(admitted) || !commitRetained(&scope, &admitted)) {
            return;
        }
        localEdits.clear(); // The initial full native baseline includes their final bank.
    }
    while (!localEdits.empty()) {
        const auto& edit = localEdits.front();
        AnchorProgress::State candidate = admitted;
        auto& bank = candidate.switches[edit.scene][edit.flag / 32];
        const uint32_t bit = uint32_t(1) << (edit.flag % 32);
        bank = edit.set ? bank | bit : bank & ~bit;
        if (!commitRetained(&scope, &candidate)) {
            return;
        }
        admitted = candidate;
        localEdits.pop_front(); // One commit per ordered event; do not collapse SET/UNSET repeats.
    }
    retained = admitted;
    if (scope.mmActive) {
        AnchorProgress::Apply(retained);
    }
#else
    if (!isEnabled) {
        return;
    }
    Scope next;
    if (!GetScope(next)) {
        return;
    }
    const bool changed = next.ownerGeneration != scope.ownerGeneration || std::strcmp(next.seed, scope.seed) != 0;
    const bool ownerChanged = next.mmActive != scope.mmActive || refreshOwner;
    if (changed) {
        ResetProtocol();
        retained = {};
        retainedOwls = {};
        pendingOwls = 0;
        pendingOwlApply = false;
        pendingApply = false;
        localEdits.clear();
        localQueueFailed = false;
        scope = next;
        if (readRetained != nullptr && !readRetained(&scope, &retained)) {
            return;
        }
    }
    scope = next;
    if (localQueueFailed)
        return;
    if (changed || ownerChanged) {
        authoritative = owlAuthoritative = false;
        stage = {};
        incomingEdits.clear();
        request = 0;
        refreshOwner = false;
        if (clientId != 0) {
            Send({ { "type", "UPDATE_CLIENT_STATE" }, { "state", ClientState() } });
            RequestState();
        }
    }
    const uint64_t connectedGeneration = connectionGeneration.load();
    if (!isConnected) {
        if (clientId != 0) {
            ResetProtocol();
        }
        return;
    }
    if (generation != connectedGeneration) {
        generation = connectedGeneration;
        ResetProtocol();
        Handshake();
    }
    std::queue<Json> packets;
    SwapIncomingPacketQueue(packets);
    while (!packets.empty()) {
        Receive(packets.front());
        packets.pop();
    }
    const double now = Now();
    if (clientId != 0 && epoch.empty() && now - lastHello >= 5) {
        std::ostringstream token;
        token << std::hex << generation << '-' << ++requestSerial;
        nonce = token.str();
        lastHello = now;
        Send({ { "type", "DIPTYCH_METADATA_HELLO" }, { "targetClientId", 0 }, { "nonce", nonce }, { "cap", 4 } });
    }
    if (!sharing || capability < 3) {
        if (pendingOwlApply && scope.mmActive &&
            OwlAccess::Apply(retainedOwls) == AnchorProgress::ApplyResult::Applied) {
            pendingOwlApply = false;
        }
        if (pendingApply && scope.mmActive && AnchorProgress::Apply(retained) == AnchorProgress::ApplyResult::Applied) {
            pendingApply = false;
        }
        return;
    }
    if ((!authoritative || (capability >= 4 && !owlAuthoritative)) && request == 0 && now - lastRequest >= 1) {
        RequestState();
    } else if (request != 0 && now - lastRequest >= 10) {
        RequestState();
    }
    FinishState();
    while (authoritative && request == 0 && !incomingEdits.empty()) {
        const auto& packet = incomingEdits.front();
        const uint64_t seq = packet["seq"].get<uint64_t>();
        if (seq <= sequence) {
            incomingEdits.pop_front();
            continue;
        }
        if (seq != sequence + 1) {
            RequestState();
            break;
        }
        AnchorProgress::State candidate = retained;
        OwlAccess::State candidateOwls = retainedOwls;
        if (!SwitchRows(packet, candidate, false) || !OwlRows(packet, candidateOwls, false, capability)) {
            RequestState();
            break;
        }
        if (!Commit(candidate) || !CommitOwlState(candidateOwls)) {
            break;
        }
        if (awaitingLocalEcho != 0 && packet.contains("sourceClientId") &&
            Number(packet["sourceClientId"], UINT32_MAX) && packet["sourceClientId"] == clientId &&
            packet["mmSwitches"].size() == awaitingLocalEcho) {
            bool matches = true;
            for (size_t i = 0; i < awaitingLocalEcho; ++i) {
                const auto& edit = localEdits[i];
                matches &= packet["mmSwitches"][i] == Json::array({ edit.scene, edit.flag, edit.set });
            }
            if (matches) {
                localEdits.erase(localEdits.begin(), localEdits.begin() + awaitingLocalEcho);
                awaitingLocalEcho = 0;
            }
        }
        sequence = seq;
        incomingEdits.pop_front();
    }
    if (awaitingLocalEcho != 0 && now - lastLocalSend >= 10) {
        RequestState(); // Recover through the existing snapshot barrier, then replay unacknowledged local rows.
    }
    FlushLocal();
    FlushOwls();
    if (pendingOwlApply && scope.mmActive && capability >= 4 && request == 0 && owlAuthoritative &&
        OwlAccess::Apply(retainedOwls) == AnchorProgress::ApplyResult::Applied) {
        pendingOwlApply = false;
    }
    if (pendingApply && scope.mmActive && localEdits.empty() && authoritative && request == 0) {
        if (AnchorProgress::Apply(retained) == AnchorProgress::ApplyResult::Applied) {
            pendingApply = false;
        }
    }
#endif
}

bool Anchor::CommitOwlState(const OwlAccess::State& state) {
    if (!state.known) {
        return true; // UNKNOWN never removes an earned local access fact.
    }
    if (commitOwls != nullptr && !commitOwls(&scope, &state)) {
        return false;
    }
    retainedOwls.known = true;
    retainedOwls.mask |= state.mask;
    pendingOwlApply = true;
    // Only genuine admitted state, never transport queue acceptance, retires offered facts.
    pendingOwls &= ~state.mask;
    return true;
}

void Anchor::LocalOwls(uint16_t mask) {
    Scope current;
    if (!GetScope(current)) {
#ifdef DIPTYCH_GAME_MODULE
        if (!SameNativeOwner()) {
            return;
        }
        current = scope;
#else
        return;
#endif
    }
    if (!current.mmActive) {
        return;
    }
    if (std::strcmp(current.seed, scope.seed) != 0 || current.ownerGeneration != scope.ownerGeneration) {
        ResetProtocol();
        scope = current;
        retained = {};
        retainedOwls = {};
        pendingApply = pendingOwlApply = false;
        pendingOwls = 0;
        localEdits.clear();
        localQueueFailed = false;
    }
    pendingOwls |= mask & OwlAccess::MASK;
}

void Anchor::PumpOwls() {
    if (readOwls == nullptr || commitOwls == nullptr || !scope.mmActive) {
        return;
    }
    OwlAccess::State admitted;
    if (!readOwls(&scope, &admitted)) {
        return;
    }
    OwlAccess::State captured;
    if (!OwlAccess::Capture(captured)) {
        return;
    }
    const uint16_t offered = pendingOwls | captured.mask;
    OwlAccess::State candidate{ true, uint16_t(admitted.mask | offered) };
    if ((!admitted.known || (offered & ~admitted.mask) != 0) && !commitOwls(&scope, &candidate)) {
        pendingOwls |= offered;
        return;
    }
    pendingOwls = 0;
    retainedOwls = candidate;
    pendingOwlApply = OwlAccess::Apply(retainedOwls) != AnchorProgress::ApplyResult::Applied;
}

void Anchor::FlushOwls() {
    if (capability < 4 || !owlAuthoritative || request != 0 || !scope.mmActive) {
        return;
    }
    OwlAccess::State captured;
    if (OwlAccess::Capture(captured)) {
        pendingOwls |= captured.mask & ~retainedOwls.mask;
    }
    if (pendingOwls == 0 || Now() - lastOwlSend < 1) {
        return;
    }
    auto packet = Envelope("DIPTYCH_METADATA_EDIT");
    packet.update({ { "checks", Json::array() },
                    { "entrances", Json::array() },
                    { "ootSwitches", Json::array() },
                    { "mmSwitches", Json::array() },
                    { "mmOwls", pendingOwls } });
    if (Send(std::move(packet))) {
        lastOwlSend = Now();
    }
}

bool Anchor::FlushSessionEdits() {
    if (localQueueFailed) {
        return false;
    }
#ifdef DIPTYCH_GAME_MODULE
    if (!localEdits.empty() || pendingOwls != 0) {
        try {
            Pump();
        } catch (const std::exception& error) {
            SPDLOG_ERROR("Anchor local progress could not be admitted: {}", error.what());
            return false;
        }
    }
    return localEdits.empty() && pendingOwls == 0;
#else
    return true; // Standalone disk persistence remains owned by normal native saving.
#endif
}

bool Anchor::CanSetSharing() const {
    return isConnected && clientId != 0 && ownerClientId == clientId && room != "soh-global";
}

bool Anchor::SetSharing(bool on) {
    if (!CanSetSharing() || !Send({ { "type", "UPDATE_ROOM_STATE" },
                                    { "state",
                                      { { "ownerClientId", clientId },
                                        { "syncItemsAndFlags", on ? 1 : 0 },
                                        { "pvpMode", 0 },
                                        { "showLocationsMode", 0 },
                                        { "teleportMode", 0 } } } })) {
        return false;
    }
    sharing = on; // Native room updates exclude the sender; successful queueing is its own control path.
    authoritative = owlAuthoritative = false;
    stage = {};
    incomingEdits.clear();
    awaitingLocalEcho = 0;
    request = 0;
    if (on) {
        RequestState();
    }
    return true;
}

const char* Anchor::Status() const {
    if (localQueueFailed) {
        return "Local progress could not be retained; sharing stopped";
    }
#ifdef DIPTYCH_GAME_MODULE
    if (scopeProvider == nullptr || readRetained == nullptr || commitRetained == nullptr) {
        return "Combined-session progress sharing is unavailable";
    }
    return retained.known ? "Using manager-admitted permanent progress" : "Waiting for manager admission";
#else
    if (!isEnabled)
        return "Disconnected";
    if (!isConnected)
        return "Connecting";
    if (!scope.admitted)
        return "Waiting for an admitted save";
    if (capability < 3)
        return "Server does not support MM permanent progress";
    if (!sharing)
        return "Room progress sharing is off";
    if (capability < 4) {
        return authoritative ? "Sharing switches; server does not support owl access" : "Synchronizing world switches";
    }
    return authoritative && owlAuthoritative ? "Sharing switches and owl access" : "Synchronizing permanent progress";
#endif
}
#endif
