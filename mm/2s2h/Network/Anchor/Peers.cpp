#include "Peers.h"
#ifdef ENABLE_ANCHOR
#include "2s2h/Rando/Rando.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/NameTag/NameTag.h"
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64malloc.h"
#include "2s2h/BenGui/HumanTunic.h"
#include "2s2h/BenGui/FormTunic.h"
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
uint8_t Player_IsCustomLinkModel(Player* player);
void Player_Draw(Actor* actor, PlayState* play);
void Player_DrawGameplay(PlayState* play, Player* player, s32 lod, Gfx* cullDList,
                         OverrideLimbDrawFlex overrideLimbDraw);
}

namespace AnchorPeers {
namespace {
using Json = nlohmann::json;
constexpr int kSceneNamespace = 1000;
constexpr size_t kMaxPeers = 256;
constexpr auto kPoseInterval = std::chrono::milliseconds(50);
constexpr auto kPoseExpiry = std::chrono::seconds(2);

using Color = std::array<uint8_t, 3>;

struct TunicMaterials {
    CosmeticHumanTunicMaterials human{};
    CosmeticFormTunicMaterials form{};
    std::unique_ptr<CosmeticFormTunicCache, decltype(&CosmeticEditor_DestroyFormTunic)> cache{
        nullptr, CosmeticEditor_DestroyFormTunic
    };
};

struct Client {
    std::string name;
    std::string team;
    bool online = false;
    bool loaded = false;
    bool mm = false;
    bool hasPose = false;
    std::chrono::steady_clock::time_point lastPose{};
    int scene = SCENE_MAX;
    int room = -1;
    Player pose{};
    std::optional<Color> color;
    TunicMaterials tunic;

    Player* actor = nullptr;
};

std::map<uint32_t, Client> sClients;
struct ActorOwner {
    uint32_t id;
    bool initialized = false;
};
std::map<Actor*, ActorOwner> sActors;
ReadSession sRead = nullptr;
Send sSend = nullptr;
Publish sPublish = nullptr;
Session sSession;
bool sSuspended = false;
uint32_t sSelf = 0;
uint32_t sSpawning = 0;
std::string sPresence;
std::optional<Color> sOwnColor;
TunicMaterials sOwnTunic;
Actor* sWrappedPlayer = nullptr;
ActorFunc sWrappedDraw = nullptr;
auto sLastPose = std::chrono::steady_clock::time_point{};

#define POSE_FIELDS(X)                                                                                          \
    X(currentMask)                                                                                              \
    X(rightHandType)                                                                                            \
    X(leftHandType) X(currentShield) X(sheathType) X(heldItemAction) X(heldItemId) X(itemAction) X(stateFlags1) \
        X(stateFlags2) X(stateFlags3) X(unk_B28) X(unk_ACC) X(invincibilityTimer)

template <class T> T Number(const Json& json, const char* key) {
    const auto& value = json.at(key);
    if (!value.is_number_integer())
        throw std::runtime_error("non-integer player field");
    const auto number = value.get<int64_t>();
    if (number < std::numeric_limits<T>::lowest() || number > std::numeric_limits<T>::max())
        throw std::runtime_error("player field out of range");
    return static_cast<T>(number);
}

float Coordinate(const Json& json, const char* key) {
    const float value = json.at(key).get<float>();
    if (!std::isfinite(value) || std::abs(value) > 1000000.0f)
        throw std::runtime_error("invalid player coordinate");
    return value;
}

Client* ActorClient(Actor* actor) {
    const auto found = sActors.find(actor);
    if (found == sActors.end())
        return nullptr;
    const auto client = sClients.find(found->second.id);
    return client != sClients.end() ? &client->second : nullptr;
}

bool Visible(const Client& client, PlayState* play) {
    return play != nullptr && play->state.running && gSaveContext.gameMode == GAMEMODE_NORMAL &&
           gSaveContext.fileNum != 0xFF && !sSuspended && sSession.connected && sSession.active && client.online &&
           client.loaded && client.mm && client.hasPose &&
           std::chrono::steady_clock::now() - client.lastPose < kPoseExpiry && client.scene == play->sceneId &&
           client.room == play->roomCtx.curRoom.num;
}

void DestroyPlayer(Actor* actor, PlayState* play) {
    if (auto client = ActorClient(actor); client != nullptr && client->actor == reinterpret_cast<Player*>(actor))
        client->actor = nullptr;
    NameTag_RemoveAllForActor(actor);
    auto player = reinterpret_cast<Player*>(actor);
    if (const auto owner = sActors.find(actor); owner != sActors.end() && owner->second.initialized) {
        Collider_DestroyCylinder(play, &player->cylinder);
        Collider_DestroyCylinder(play, &player->shieldCylinder);
        Collider_DestroyQuad(play, &player->meleeWeaponQuads[0]);
        Collider_DestroyQuad(play, &player->meleeWeaponQuads[1]);
        Collider_DestroyQuad(play, &player->shieldQuad);
        if (player->maskObjectSegment != nullptr)
            ZeldaArena_Free(player->maskObjectSegment);
        player->maskObjectSegment = nullptr;
    }
    sActors.erase(actor);
}

void UpdatePlayer(Actor* actor, PlayState* play) {
    auto client = ActorClient(actor);
    auto player = reinterpret_cast<Player*>(actor);
    if (client == nullptr || !Visible(*client, play) || player->transformation != client->pose.transformation) {
        Actor_Kill(actor);
        return;
    }
    const auto& pose = client->pose;
    actor->world.pos = pose.actor.world.pos;
    actor->shape.rot = pose.actor.shape.rot;
    player->yaw = actor->shape.rot.y;
    memcpy(player->jointTableBuffer, pose.jointTableBuffer, sizeof(player->jointTableBuffer));
    memcpy(player->jointTableUpperBuffer, pose.jointTableUpperBuffer, sizeof(player->jointTableUpperBuffer));
    player->maskId = player->currentMask;
    player->currentMask = pose.currentMask;
    player->maskObjectLoadState = 0;
#define COPY_FIELD(name) player->name = pose.name;
    POSE_FIELDS(COPY_FIELD)
#undef COPY_FIELD
    player->unk_B0C = pose.unk_B0C;
    const auto modelAction =
        player->itemAction < 0 ? PLAYER_IA_NONE : static_cast<PlayerItemAction>(player->itemAction);
    Player_SetModels(player, Player_ActionToModelGroup(player, modelAction));
}

s32 OverridePlayerLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* actor) {
    const s32 handled = Player_OverrideLimbDrawGameplayDefault(play, limbIndex, dList, pos, rot, actor);
    if (!handled) {
        if (const auto client = ActorClient(actor); client != nullptr) {
            *dList = CosmeticEditor_HumanTunicDList(&client->tunic.human, *dList);
            *dList = CosmeticEditor_FormTunicDList(&client->tunic.form, *dList);
        }
    }
    return handled;
}

bool PrepareTunic(PlayState* play, Player* player, const std::optional<Color>& color, TunicMaterials& tunic) {
    tunic.human = {};
    tunic.form = {};
    if (!color || play == nullptr || play->state.gfxCtx == nullptr || Player_IsCustomLinkModel(player))
        return false;
    const auto& rgb = *color;
    const Color_RGBA8 tint{ rgb[0], rgb[1], rgb[2], 255 };
    if (player->transformation == PLAYER_FORM_HUMAN)
        return CosmeticEditor_BuildHumanTunic(play, tint, &tunic.human) != 0;
    if (!tunic.cache)
        tunic.cache.reset(CosmeticEditor_CreateFormTunic());
    return CosmeticEditor_BuildFormTunic(play, tunic.cache.get(), player->transformation, tint, &tunic.form) != 0;
}

void DrawPlayer(Actor* actor, PlayState* play) {
    const auto client = ActorClient(actor);
    if (client != nullptr && Visible(*client, play) &&
        reinterpret_cast<Player*>(actor)->transformation == client->pose.transformation) {
        auto player = reinterpret_cast<Player*>(actor);
        const bool colored = PrepareTunic(play, player, client->color, client->tunic);
        Gfx* opaqueBegin = play->state.gfxCtx->polyOpa.p;
        Player_DrawGameplay(play, player, 1, gCullBackDList,
                            colored ? OverridePlayerLimb : Player_OverrideLimbDrawGameplayDefault);
        if (colored && player->transformation == PLAYER_FORM_ZORA)
            CosmeticEditor_FormTunicPostDraw(&client->tunic.form, opaqueBegin, play->state.gfxCtx->polyOpa.p);
        // Copied commands live in the native graphics arena through deferred rendering.
        client->tunic.human = {};
        client->tunic.form = {};
    }
}

void DrawLocalPlayer(Actor* actor, PlayState* play) {
    const auto originalDraw = sWrappedDraw;
    if (actor != sWrappedPlayer || originalDraw == nullptr)
        return;
    const bool colored = PrepareTunic(play, reinterpret_cast<Player*>(actor), sOwnColor, sOwnTunic);
    Gfx* opaqueBegin = play->state.gfxCtx->polyOpa.p;
    Gfx* translucentBegin = play->state.gfxCtx->polyXlu.p;
    // Preserve the full native draw and all actor hooks; substitute only this actor's stock materials.
    originalDraw(actor, play);
    if (colored) {
        CosmeticEditor_TunicPostDraw(&sOwnTunic.human, &sOwnTunic.form, opaqueBegin, play->state.gfxCtx->polyOpa.p);
        CosmeticEditor_TunicPostDraw(&sOwnTunic.human, &sOwnTunic.form, translucentBegin,
                                     play->state.gfxCtx->polyXlu.p);
    }
    sOwnTunic.human = {};
    sOwnTunic.form = {};
}

void RestoreLocalDraw() {
    // Never dereference a saved actor pointer after scene destruction.
    auto play = gPlayState;
    if (play != nullptr && GET_PLAYER(play) != nullptr) {
        auto actor = &GET_PLAYER(play)->actor;
        if (actor == sWrappedPlayer && actor->draw == DrawLocalPlayer)
            actor->draw = sWrappedDraw;
    }
    sWrappedPlayer = nullptr;
    sWrappedDraw = nullptr;
}

void InitPlayer(Actor* actor, PlayState* play) {
    auto client = ActorClient(actor);
    if (client == nullptr || !Visible(*client, play) || play->playerInit == nullptr) {
        Actor_Kill(actor);
        return;
    }
    auto player = reinterpret_cast<Player*>(actor);
    player->transformation = client->pose.transformation;
    player->ageProperties = &sPlayerAgeProperties[player->transformation];
    actor->room = -1;
    player->csId = CS_ID_NONE;
    player->heldItemAction = PLAYER_IA_NONE;
    player->heldItemId = ITEM_NONE;
    Player_SetModelGroup(player, PLAYER_MODELGROUP_DEFAULT);
    play->playerInit(player, play, gPlayerSkeletons[player->transformation]);
    sActors.at(actor).initialized = true;
    player->maskObjectSegment = ZeldaArena_Malloc(0x3800);
    if (player->maskObjectSegment == nullptr) {
        Actor_Kill(actor);
        return;
    }
    actor->flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED |
                   ACTOR_FLAG_INSIDE_CULLING_VOLUME | ACTOR_FLAG_LOCK_ON_DISABLED;
    UpdatePlayer(actor, play);
    NameTag_RegisterForActorWithOptions(actor, client->name.c_str(), { .yOffset = 30 });
}

void KillPlayers() {
    for (const auto& [actor, owner] : sActors)
        Actor_Kill(actor);
    for (auto& [id, client] : sClients)
        client.actor = nullptr;
}

bool Unsigned(const Json& value, uint64_t max) {
    return value.is_number_integer() && (value.is_number_unsigned() || value.get<int64_t>() >= 0) &&
           value.get<uint64_t>() <= max;
}

bool Text(const Json& state, const char* key, std::string& result, size_t max) {
    const auto field = state.find(key);
    if (field == state.end()) {
        result.clear();
        return true;
    }
    if (!field->is_string())
        return false;
    const auto& text = field->get_ref<const std::string&>();
    if (text.size() > max || text.find(char(0)) != std::string::npos)
        return false;
    result = text;
    return true;
}

struct Presence {
    uint32_t id = 0;
    std::string name, team;
    bool online = false, loaded = false, mm = false;
    int scene = SCENE_MAX, room = -1;
    std::optional<Color> color;
};

bool DecodePresence(uint32_t id, const Json& state, Presence& result) {
    if (id == 0 || !state.is_object())
        return false;
    Presence next;
    next.id = id;
    if (!Text(state, "name", next.name, 256) || !Text(state, "teamId", next.team, 64))
        return false;
    for (const char* flag : { "online", "isSaveLoaded", "self" })
        if (state.contains(flag) && !state[flag].is_boolean())
            return false;
    next.online = state.value("online", false);
    next.loaded = state.value("isSaveLoaded", false);
    if (state.contains("color")) {
        const auto& color = state["color"];
        if (!color.is_object())
            return false;
        Color rgb{};
        size_t index = 0;
        for (const char* channel : { "r", "g", "b" }) {
            if (!color.contains(channel) || !Unsigned(color[channel], 255))
                return false;
            rgb[index++] = color[channel].get<uint8_t>();
        }
        next.color = rgb;
    }
    if (state.contains("diptych")) {
        const auto& game = state["diptych"];
        if (!game.is_object() || !game.contains("game") || !game["game"].is_string())
            return false;
        next.mm = game["game"] == "mm";
    } else if (state.contains("sceneId") && state.contains("sceneNum") && Unsigned(state["sceneId"], SCENE_MAX) &&
               Unsigned(state["sceneNum"], kSceneNamespace + SCENE_MAX)) {
        next.mm = state["sceneNum"].get<int>() == kSceneNamespace + state["sceneId"].get<int>();
    }
    if (next.mm && next.loaded) {
        if (!state.contains("sceneId") || !Unsigned(state["sceneId"], SCENE_MAX - 1) || !state.contains("sceneNum") ||
            !Unsigned(state["sceneNum"], kSceneNamespace + SCENE_MAX - 1) || !state.contains("curRoomNum"))
            return false;
        next.scene = state["sceneId"].get<int>();
        if (state["sceneNum"].get<int>() != kSceneNamespace + next.scene)
            return false;
        next.room = Number<int8_t>(state, "curRoomNum");
    }
    result = std::move(next);
    return true;
}

void ApplyPresence(const Presence& next) {
    if (!sClients.contains(next.id) && sClients.size() >= kMaxPeers)
        return;
    auto& client = sClients[next.id];
    if (next.scene != client.scene || next.room != client.room || !next.mm || !next.loaded || !next.online)
        client.hasPose = false;
    if (client.actor != nullptr && client.name != next.name) {
        NameTag_RemoveAllForActor(&client.actor->actor);
        NameTag_RegisterForActorWithOptions(&client.actor->actor, next.name.c_str(), { .yOffset = 30 });
    }
    client.name = next.name;
    client.team = next.team;
    client.online = next.online;
    client.loaded = next.loaded;
    client.mm = next.mm;
    client.scene = next.scene;
    client.room = next.room;
    client.color = next.color;
}

void InvalidatePoses() {
    RestoreLocalDraw();
    KillPlayers();
    for (auto& [id, client] : sClients)
        client.hasPose = false;
    sPresence.clear();
    sLastPose = {};
}

bool SyncSession() {
    Session next;
    if (sRead == nullptr || !sRead(&next) || std::memchr(next.room, 0, sizeof(next.room)) == nullptr ||
        std::memchr(next.team, 0, sizeof(next.team)) == nullptr)
        next = {};
    const bool changed = next.generation != sSession.generation || next.clientId != sSession.clientId ||
                         next.connected != sSession.connected || std::strcmp(next.room, sSession.room) != 0 ||
                         std::strcmp(next.team, sSession.team) != 0;
    if (changed) {
        InvalidatePoses();
        sClients.clear();
    } else if (next.active != sSession.active || next.ownerGeneration != sSession.ownerGeneration) {
        InvalidatePoses();
    }
    if (next.color.r != sSession.color.r || next.color.g != sSession.color.g || next.color.b != sSession.color.b)
        sPresence.clear();
    sSession = next;
    sSelf = next.clientId;
    sOwnColor.reset();
    if (next.connected && next.active && !sSuspended)
        sOwnColor = Color{ next.color.r, next.color.g, next.color.b };
    else
        RestoreLocalDraw();
    return next.connected;
}

bool Loaded() {
    return !sSuspended && sSession.active && gPlayState != nullptr && gPlayState->state.running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.fileNum != 0xFF &&
           GET_PLAYER(gPlayState) != nullptr && gPlayState->sceneId >= 0 && gPlayState->sceneId < SCENE_MAX;
}

void ReadPose(uint32_t id, const Json& packet) {
    const auto found = sClients.find(id);
    if (found == sClients.end() || id == sSelf || !found->second.mm || !found->second.online || !found->second.loaded ||
        !Loaded() || !packet.contains("targetClientId") || !Unsigned(packet["targetClientId"], UINT32_MAX) ||
        packet["targetClientId"] != sSelf || (packet.contains("originGame") && packet["originGame"] != "mm"))
        return;
    Player pose{};
    pose.transformation = Number<uint8_t>(packet, "transformation");
    if (pose.transformation >= PLAYER_FORM_MAX)
        return;
    const auto& position = packet.at("posRot").at("pos");
    pose.actor.world.pos = { Coordinate(position, "x"), Coordinate(position, "y"), Coordinate(position, "z") };
    const auto& rotation = packet.at("posRot").at("rot");
    pose.actor.shape.rot = { Number<s16>(rotation, "x"), Number<s16>(rotation, "y"), Number<s16>(rotation, "z") };
    const auto& joints = packet.at("jointTable");
    const auto& upper = packet.at("upperJointTable");
    if (!joints.is_array() || !upper.is_array() || joints.size() != sizeof(pose.jointTableBuffer) ||
        upper.size() != sizeof(pose.jointTableUpperBuffer))
        return;
    for (size_t i = 0; i < joints.size(); ++i) {
        if (!Unsigned(joints[i], 255) || !Unsigned(upper[i], 255))
            return;
        pose.jointTableBuffer[i] = joints[i].get<u8>();
        pose.jointTableUpperBuffer[i] = upper[i].get<u8>();
    }
#define READ_FIELD(name) pose.name = Number<decltype(pose.name)>(packet, #name);
    POSE_FIELDS(READ_FIELD)
#undef READ_FIELD
    pose.unk_B0C = Coordinate(packet, "unk_B0C");
    if (pose.currentMask >= PLAYER_MASK_MAX || pose.rightHandType >= PLAYER_MODELTYPE_MAX ||
        pose.leftHandType >= PLAYER_MODELTYPE_MAX || pose.sheathType >= PLAYER_MODELTYPE_MAX ||
        pose.currentShield < 0 || pose.currentShield > 2 || pose.itemAction < PLAYER_IA_MINUS1 ||
        pose.itemAction >= PLAYER_IA_MAX || pose.heldItemAction < PLAYER_IA_MINUS1 ||
        pose.heldItemAction >= PLAYER_IA_MAX)
        return;
    const int scene = Number<s16>(packet, "sceneId");
    const int room = Number<s8>(packet, "roomIndex");
    if (scene < 0 || scene >= SCENE_MAX)
        return;
    auto& client = found->second;
    // Presence owns location; a delayed pose cannot bring a departed player back.
    if (scene != client.scene || room != client.room)
        return;
    client.pose = pose;
    client.hasPose = true;
    client.lastPose = std::chrono::steady_clock::now();
}

Json LegacyPose(Json packet) {
    packet["transformation"] = packet.at("form");
    packet["sceneId"] = packet.at("scene");
    packet["roomIndex"] = packet.at("room");
    packet["posRot"] = { { "pos", packet.at("pos") }, { "rot", packet.at("rot") } };
    packet["jointTable"] = packet.at("joints");
    packet["upperJointTable"] = packet.at("upper");
    return packet;
}

void PublishState() {
    if (sPublish == nullptr || !sSession.connected)
        return;
    const bool loaded = Loaded();
    const int scene = loaded ? gPlayState->sceneId : SCENE_MAX;
    const Json state = { { "seed", loaded && IS_RANDO ? gSaveContext.save.shipSaveInfo.rando.finalSeed : 0 },
                         { "paired", loaded && sSession.paired },
                         { "isSaveLoaded", loaded },
                         { "isGameComplete", false },
                         { "sceneNum", kSceneNamespace + scene },
                         { "sceneId", scene },
                         { "curRoomNum", loaded ? gPlayState->roomCtx.curRoom.num : -1 },
                         { "entranceIndex", kSceneNamespace },
                         { "diptych", { { "proto", 1 }, { "seed", "" }, { "game", "mm" } } } };
    const auto wire = state.dump();
    if (wire != sPresence && sPublish(wire.c_str()))
        sPresence = wire;
}

void SendPose(PlayState* play) {
    const auto now = std::chrono::steady_clock::now();
    if (sSend == nullptr || now - sLastPose < kPoseInterval)
        return;
    sLastPose = now;
    const auto* player = GET_PLAYER(play);
    Json packet = { { "type", "PLAYER_UPDATE" },
                    { "quiet", true },
                    { "transformation", player->transformation },
                    { "sceneId", play->sceneId },
                    { "entrance", gSaveContext.save.entrance },
                    { "roomIndex", play->roomCtx.curRoom.num },
                    { "posRot",
                      { { "pos",
                          { { "x", player->actor.world.pos.x },
                            { "y", player->actor.world.pos.y },
                            { "z", player->actor.world.pos.z } } },
                        { "rot",
                          { { "x", player->actor.shape.rot.x },
                            { "y", player->actor.shape.rot.y },
                            { "z", player->actor.shape.rot.z } } } } },
                    { "jointTable",
                      std::vector<int>(std::begin(player->jointTableBuffer), std::end(player->jointTableBuffer)) },
                    { "upperJointTable", std::vector<int>(std::begin(player->jointTableUpperBuffer),
                                                          std::end(player->jointTableUpperBuffer)) },
                    { "unk_B0C", player->unk_B0C } };
#define WRITE_FIELD(name) packet[#name] = player->name;
    POSE_FIELDS(WRITE_FIELD)
#undef WRITE_FIELD
    for (const auto& [id, client] : sClients) {
        if (id == sSelf || !client.online || !client.loaded || !client.mm || client.scene != play->sceneId ||
            client.room != play->roomCtx.curRoom.num)
            continue;
        packet["targetClientId"] = id;
        sSend(packet.dump().c_str());
    }
}

void Frame() {
    if (!SyncSession())
        return;
    for (auto it = sClients.begin(); it != sClients.end();) {
        if (!it->second.online && it->second.actor == nullptr)
            it = sClients.erase(it);
        else
            ++it;
    }
    PublishState();
    auto* play = gPlayState;
    if (sSelf == 0 || !Loaded())
        return;
    for (auto& [id, client] : sClients) {
        if (id == sSelf || client.actor != nullptr || !Visible(client, play))
            continue;
        bool destroying = false;
        for (const auto& [actor, owner] : sActors)
            if (owner.id == id) {
                destroying = true;
                break;
            }
        if (destroying)
            continue;
        sSpawning = id;
        auto* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_PLAYER, client.pose.actor.world.pos.x,
                                  client.pose.actor.world.pos.y, client.pose.actor.world.pos.z, 0, 0, 0, 0);
        sSpawning = 0;
        client.actor = reinterpret_cast<Player*>(actor);
    }
    SendPose(play);
}
} // namespace

void SetTransport(ReadSession read, Send send, Publish publish) {
    if (sRead == read && sSend == send && sPublish == publish)
        return;
    InvalidatePoses();
    sClients.clear();
    sSession = {};
    sOwnColor.reset();
    if (read != nullptr)
        sSuspended = false;
    sRead = read;
    sSend = send;
    sPublish = publish;
}

void Receive(const char* wire) {
    if (wire == nullptr || !SyncSession())
        return;
    const size_t length = strnlen(wire, 1024 * 1024 + 1);
    if (length > 1024 * 1024)
        return;
    const auto packet = Json::parse(wire, wire + length, nullptr, false);
    if (!packet.is_object() || !packet.contains("type") || !packet["type"].is_string())
        return;
    try {
        const auto type = packet["type"].get<std::string>();
        if (type == "ALL_CLIENT_STATE") {
            if (!packet.contains("state") || !packet["state"].is_array() || packet["state"].size() > kMaxPeers)
                return;
            std::map<uint32_t, Presence> staged;
            for (const auto& state : packet["state"]) {
                if (!state.is_object() || !state.contains("clientId") || !Unsigned(state["clientId"], UINT32_MAX))
                    return;
                Presence next;
                const uint32_t id = state["clientId"].get<uint32_t>();
                if (!DecodePresence(id, state, next) || staged.contains(id))
                    return;
                staged.emplace(id, std::move(next));
            }
            for (auto& [id, client] : sClients) {
                if (!staged.contains(id)) {
                    client.online = false;
                    client.hasPose = false;
                }
            }
            for (const auto& [id, presence] : staged)
                ApplyPresence(presence);
            for (auto it = sClients.begin(); it != sClients.end();) {
                if (!it->second.online && it->second.actor == nullptr)
                    it = sClients.erase(it);
                else
                    ++it;
            }
            sPresence.clear();
        } else if (type == "UPDATE_CLIENT_STATE") {
            if (!packet.contains("clientId") || !Unsigned(packet["clientId"], UINT32_MAX) || !packet.contains("state"))
                return;
            Presence next;
            if (DecodePresence(packet["clientId"].get<uint32_t>(), packet["state"], next))
                ApplyPresence(next);
        } else if (type == "PLAYER_UPDATE" || type == "DIPTYCH_PLAYER") {
            if (!packet.contains("clientId") || !Unsigned(packet["clientId"], UINT32_MAX))
                return;
            ReadPose(packet["clientId"].get<uint32_t>(), type == "DIPTYCH_PLAYER" ? LegacyPose(packet) : packet);
        }
    } catch (const std::exception&) {
        // An invalid remote packet never changes the last usable presence or pose.
    }
}

void Init() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;
    auto* interactor = GameInteractor::Instance;
    interactor->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() { Frame(); });
    interactor->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { InvalidatePoses(); });
    interactor->RegisterGameHook<GameInteractor::OnSceneInit>([](s16, s8) { InvalidatePoses(); });
    interactor->RegisterGameHook<GameInteractor::OnRoomInit>([](s16, s8) { InvalidatePoses(); });
    interactor->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        // The native actor cleanup has already run; no saved actor pointer may be dereferenced here.
        sActors.clear();
        for (auto& [id, client] : sClients) {
            client.actor = nullptr;
            client.hasPose = false;
        }
        sWrappedPlayer = nullptr;
        sWrappedDraw = nullptr;
        sPresence.clear();
    });
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorInit>(ACTOR_PLAYER, [](Actor* actor, bool*) {
        if (sSpawning == 0)
            return;
        sActors[actor] = { sSpawning };
        Actor_ChangeCategory(gPlayState, &gPlayState->actorCtx, actor, ACTORCAT_NPC);
        actor->id = ACTOR_ITEM_INBOX;
        actor->init = InitPlayer;
        actor->update = UpdatePlayer;
        actor->draw = DrawPlayer;
        actor->destroy = DestroyPlayer;
    });
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(ACTOR_PLAYER, [](Actor* actor, bool* should) {
        auto* play = gPlayState;
        if (!*should || !sOwnColor || play == nullptr || GET_PLAYER(play) == nullptr ||
            actor != &GET_PLAYER(play)->actor || (actor->draw != PlayerCall_Draw && actor->draw != Player_Draw))
            return;
        sWrappedDraw = actor->draw;
        actor->draw = DrawLocalPlayer;
        sWrappedPlayer = actor;
    });
    interactor->RegisterGameHookForID<GameInteractor::OnActorDestroy>(ACTOR_PLAYER, [](Actor* actor) {
        if (actor == sWrappedPlayer) {
            sWrappedPlayer = nullptr;
            sWrappedDraw = nullptr;
        }
    });
}

void Suspend() {
    sSuspended = true;
    sOwnColor.reset();
    InvalidatePoses();
    PublishState();
}
void Resume() {
    sSuspended = false;
    InvalidatePoses();
}
void Shutdown() {
    Suspend();
    sRead = nullptr;
    sSend = nullptr;
    sPublish = nullptr;
    sOwnTunic.cache.reset();
    sClients.clear();
    sSession = {};
    sSelf = 0;
}
} // namespace AnchorPeers
#endif
