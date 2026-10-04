"""Run native peer decoding, session and actor lifecycle bodies with game/render shells.

Run: python tests/test_anchor_peers.py --json-include <include>
This does not render game resources or establish special-action animation parity.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile

from test_save_write_results import function

ROOT = pathlib.Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include "Peers.h"
using u8=uint8_t; using s8=int8_t; using s16=int16_t; using s32=int32_t;
struct Gfx {};
struct Vec3f { float x=0,y=0,z=0; }; struct Vec3s { s16 x=0,y=0,z=0; };
struct PlayState;
using ActorFunc=void(*)(struct Actor*,PlayState*);
struct Actor {
 struct { Vec3f pos; } world;
 struct { Vec3s rot; } shape;
 int room=0,id=0,flags=0;
 ActorFunc init=nullptr,update=nullptr,draw=nullptr,destroy=nullptr;
 bool killed=false;
};
struct PlayerAgeProperties {};
struct Player {
 Actor actor;
 uint8_t transformation=4, currentMask=0,rightHandType=0,leftHandType=0,sheathType=0,heldItemId=255;
 int8_t currentShield=0,heldItemAction=0,itemAction=0;
 uint32_t stateFlags1=0,stateFlags2=0,stateFlags3=0;
 int16_t unk_B28=0,unk_ACC=0,invincibilityTimer=0;
 float unk_B0C=0;
 s16 yaw=0;
 int maskId=0,maskObjectLoadState=0,csId=0;
 uint8_t jointTableBuffer[159]{},jointTableUpperBuffer[159]{};
 PlayerAgeProperties* ageProperties=nullptr;
 void* maskObjectSegment=nullptr;
 int cylinder=0,shieldCylinder=0,meleeWeaponQuads[2]{},shieldQuad=0;
};
struct GraphicsContext { struct { Gfx* p=nullptr; } polyOpa,polyXlu; };
struct PlayState {
 struct { bool running=true; GraphicsContext* gfxCtx=nullptr; } state;
 int16_t sceneId=3;
 struct { struct { int8_t num=1; } curRoom; } roomCtx;
 int actorCtx=0;
 Player* player=nullptr;
 void(*playerInit)(Player*,PlayState*,void*)=nullptr;
};
using OverrideLimbDrawFlex=s32(*)(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,Actor*);
constexpr int SCENE_MAX=120,PLAYER_FORM_MAX=5,PLAYER_FORM_HUMAN=4,PLAYER_FORM_ZORA=2;
constexpr int PLAYER_MASK_MAX=30,PLAYER_MODELTYPE_MAX=32,PLAYER_IA_MINUS1=-1,PLAYER_IA_MAX=80,PLAYER_IA_NONE=0;
constexpr int ITEM_NONE=255,CS_ID_NONE=-1,PLAYER_MODELGROUP_DEFAULT=0,GAMEMODE_NORMAL=0;
constexpr int ACTOR_PLAYER=0,ACTOR_ITEM_INBOX=158,ACTORCAT_NPC=4;
constexpr int ACTOR_FLAG_UPDATE_CULLING_DISABLED=1,ACTOR_FLAG_DRAW_CULLING_DISABLED=2;
constexpr int ACTOR_FLAG_INSIDE_CULLING_VOLUME=4,ACTOR_FLAG_LOCK_ON_DISABLED=8;
using PlayerItemAction=int;
struct { int fileNum=1,gameMode=0; struct { int entrance=0; struct { struct { int finalSeed=7; } rando; } shipSaveInfo; } save; } gSaveContext;
PlayState* gPlayState=nullptr;
#define GET_PLAYER(play) ((play)->player)
#define IS_RANDO false
#include "HumanTunic.h"
#include "FormTunic.h"
PlayerAgeProperties sPlayerAgeProperties[5]; void* gPlayerSkeletons[5]{}; Gfx* gCullBackDList=nullptr;
int spawned=0,destroyed=0,colliderDestroys=0,nativeDraws=0,colorBuilds=0,postDraws=0;
bool custom=false,failAllocation=false;
Color_RGBA8 lastColor{};
void PlayerCall_Draw(Actor*,PlayState*) { ++nativeDraws; }
void Player_Draw(Actor*,PlayState*) { ++nativeDraws; }
uint8_t Player_IsCustomLinkModel(Player*) { return custom; }
int Player_ActionToModelGroup(Player*,PlayerItemAction) { return 0; }
void Player_SetModels(Player*,int) {}
void Player_SetModelGroup(Player*,int) {}
s32 Player_OverrideLimbDrawGameplayDefault(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,Actor*) { return 0; }
void Player_DrawGameplay(PlayState*,Player*,s32,Gfx*,OverrideLimbDrawFlex) { ++nativeDraws; }
void Collider_DestroyCylinder(PlayState*,int*) { ++colliderDestroys; }
void Collider_DestroyQuad(PlayState*,int*) { ++colliderDestroys; }
void* ZeldaArena_Malloc(size_t) { return failAllocation?nullptr:static_cast<void*>(new char[1]); }
void ZeldaArena_Free(void* p) { delete[] static_cast<char*>(p); }
void NameTag_RemoveAllForActor(Actor*) {}
struct NameTagOptions { const char* tag=nullptr; int16_t yOffset=0; Color_RGBA8 textColor{}; uint8_t noZBuffer=0; };
void NameTag_RegisterForActorWithOptions(Actor*,const char*,NameTagOptions) {}
void Actor_Kill(Actor* actor) { actor->killed=true; actor->update=nullptr; actor->draw=nullptr; }
void Actor_ChangeCategory(PlayState*,int*,Actor*,int) {}
struct CosmeticFormTunicCache {};
extern "C" {
CosmeticFormTunicCache* CosmeticEditor_CreateFormTunic() { return new CosmeticFormTunicCache; }
void CosmeticEditor_DestroyFormTunic(CosmeticFormTunicCache* p) { delete p; }
int CosmeticEditor_BuildHumanTunic(PlayState*,Color_RGBA8 c,CosmeticHumanTunicMaterials*) { lastColor=c; ++colorBuilds; return 1; }
int CosmeticEditor_BuildFormTunic(PlayState*,CosmeticFormTunicCache*,u8,Color_RGBA8 c,CosmeticFormTunicMaterials*) { lastColor=c; ++colorBuilds; return 1; }
Gfx* CosmeticEditor_HumanTunicDList(const CosmeticHumanTunicMaterials*,Gfx* p) { return p; }
Gfx* CosmeticEditor_FormTunicDList(const CosmeticFormTunicMaterials*,Gfx* p) { return p; }
void CosmeticEditor_FormTunicPostDraw(const CosmeticFormTunicMaterials*,Gfx*,Gfx*) { ++postDraws; }
void CosmeticEditor_TunicPostDraw(const CosmeticHumanTunicMaterials*,const CosmeticFormTunicMaterials*,Gfx*,Gfx*) { ++postDraws; }
}
struct GameInteractor {
#define TAG(name,signature) struct name { using Fn=std::function<void signature>; inline static Fn hook; };
 TAG(OnGameStateMainStart,()) TAG(OnSaveLoad,(s16)) TAG(OnSceneInit,(s16,s8)) TAG(OnRoomInit,(s16,s8))
 TAG(OnPlayDestroy,()) TAG(ShouldActorInit,(Actor*,bool*)) TAG(ShouldActorDraw,(Actor*,bool*)) TAG(OnActorDestroy,(Actor*))
#undef TAG
 inline static GameInteractor* Instance=nullptr;
 template<class H,class F> void RegisterGameHook(F f) { H::hook=f; }
 template<class H,class F> void RegisterGameHookForID(int,F f) { H::hook=f; }
};
std::vector<Player*> allocated;
Actor* Actor_Spawn(int*,PlayState* play,int,float x,float y,float z,int,int,int,int) {
 auto* player=new Player; allocated.push_back(player); ++spawned;
 player->actor.world.pos={x,y,z}; bool should=true;
 GameInteractor::ShouldActorInit::hook(&player->actor,&should);
 if (should && player->actor.init) player->actor.init(&player->actor,play);
 player->actor.init=nullptr;
 return &player->actor;
}
void cleanup(bool all=false) {
 for(auto it=allocated.begin();it!=allocated.end();) {
  auto* player=*it;
  if(all || player->actor.killed) {
   if(player->actor.destroy) player->actor.destroy(&player->actor,gPlayState);
   GameInteractor::OnActorDestroy::hook(&player->actor);
   delete player; ++destroyed; it=allocated.erase(it);
  } else ++it;
 }
}
'''
CASES = r'''
using namespace AnchorPeers;
Session offered;
std::vector<nlohmann::json> sent,published;
bool read(Session* session) { *session=offered; return true; }
bool send(const char* wire) { sent.push_back(nlohmann::json::parse(wire)); return true; }
bool publish(const char* wire) { published.push_back(nlohmann::json::parse(wire)); return true; }
int assertions=0;
void require(bool condition,const char* name) { ++assertions; if(!condition) throw std::runtime_error(name); }
void receive(nlohmann::json packet) { Receive(packet.dump().c_str()); }
nlohmann::json presence(int id=2) {
 return {{"clientId",id},{"name","friend"},{"teamId","default"},{"online",true},{"isSaveLoaded",true},
         {"sceneId",3},{"sceneNum",1003},{"curRoomNum",1},{"color",{{"r",210},{"g",30},{"b",90}}},
         {"diptych",{{"game","mm"},{"proto",1},{"seed","paired"}}}};
}
nlohmann::json pose(int form=4) {
 nlohmann::json p={{"type","PLAYER_UPDATE"},{"clientId",2},{"targetClientId",1},{"transformation",form},
 {"sceneId",3},{"roomIndex",1},{"entrance",0},{"posRot",{{"pos",{{"x",12.5},{"y",20},{"z",30}}},
 {"rot",{{"x",1},{"y",2},{"z",3}}}}},{"jointTable",std::vector<int>(159,7)},
 {"upperJointTable",std::vector<int>(159,8)},{"unk_B0C",1.5}};
#define FIELD(name) p[#name]=0;
 POSE_FIELDS(FIELD)
#undef FIELD
 return p;
}
void roster() { receive({{"type","ALL_CLIENT_STATE"},{"state",nlohmann::json::array({presence()})}}); }
void frame() { GameInteractor::OnGameStateMainStart::hook(); }
struct ProtocolFixture {
 uint64_t clientId=1,ownerClientId=2,sequence=77;
 unsigned capability=4;
 bool sharing=true,authoritative=true,owlAuthoritative=true;
 std::string epoch="epoch",nonce="nonce";
 nlohmann::json stage={{"stale",true}};
 uint32_t request=42;size_t awaitingLocalEcho=3;
 std::vector<nlohmann::json> incomingEdits={{{"stale",true}}};
 double lastHello=9,lastRequest=9,lastOwlSend=9;
 void ResetProtocol(bool keepConnection);
};
PROTOCOL_BODY
void protocol() {
 ProtocolFixture state;
 state.ResetProtocol(true);
 require(state.clientId==1 && state.ownerClientId==2 && state.sharing,"scope reset retains attached room identity");
 require(state.sequence==0 && state.capability==0 && !state.authoritative && !state.owlAuthoritative,
         "scope reset clears progress authority");
 require(state.epoch.empty() && state.nonce.empty() && state.request==0 && state.awaitingLocalEcho==0 &&
         state.incomingEdits.empty(),"scope reset fences old responses and queued echoes");
 state.ResetProtocol(false);
 require(state.clientId==0 && state.ownerClientId==0 && !state.sharing,"connection reset clears room and self identity");
}
int main() {
 try {
 protocol();
 GameInteractor interactor; GameInteractor::Instance=&interactor;
 Player local; local.actor.draw=PlayerCall_Draw;
 GraphicsContext gfx; PlayState play; play.player=&local; play.state.gfxCtx=&gfx;
 play.playerInit=[](Player*,PlayState*,void*){}; gPlayState=&play;
 offered.clientId=1;offered.connected=true;offered.active=true;offered.generation=1;
 std::memcpy(offered.room,"room",5);std::memcpy(offered.team,"default",8);offered.color={13,24,35,255};
 Init();SetTransport(read,send,publish);frame();roster();receive(pose());sLastPose={};frame();
 require(spawned==1 && sClients.at(2).hasPose,"standard two-peer pose spawns once");
 auto* peer=sClients.at(2).actor;
 require(peer && peer->actor.world.pos.x==12.5f && peer->jointTableBuffer[158]==7 && peer->jointTableUpperBuffer[158]==8,"pose applied to actor");
 require(!sent.empty() && sent.back()["type"]=="PLAYER_UPDATE" && sent.back()["jointTable"].size()==159,"canonical pose emitted");
 require(published.back()["paired"]==false && published.back()["isSaveLoaded"]==true,"unpaired presence works");
 peer->actor.draw(&peer->actor,&play);
 require(lastColor.r==210 && colorBuilds==1,"peer authoritative identity color");
 bool should=true;GameInteractor::ShouldActorDraw::hook(&local.actor,&should);local.actor.draw(&local.actor,&play);
 require(nativeDraws==2 && lastColor.r==13 && postDraws==2,"local full native draw scoped color");
 custom=true;int builds=colorBuilds;local.actor.draw(&local.actor,&play);custom=false;
 require(colorBuilds==builds && nativeDraws==3,"custom model retains native fallback");
 for(int form=0;form<5;++form) {
  local.transformation=uint8_t(form);local.actor.draw(&local.actor,&play);
  require(lastColor.r==13,"all supported forms use identity RGB");
 }
 ++offered.ownerGeneration;frame();
 require(sClients.contains(2) && !sClients.at(2).hasPose,"owner change keeps same-connection roster but expires pose");
 cleanup();receive(pose());frame();
 auto old=sClients.at(2).pose.actor.world.pos.x;
 for(const char* bad : {"target","origin","scene","form","joint","model","coordinate"}) {
  auto p=pose();std::string key=bad;
  if(key=="target")p["targetClientId"]=9;
  if(key=="origin")p["originGame"]="oot";
  if(key=="scene")p["sceneId"]=4;
  if(key=="form")p["transformation"]=5;
  if(key=="joint")p["jointTable"][158]=256;
  if(key=="model")p["rightHandType"]=PLAYER_MODELTYPE_MAX;
  if(key=="coordinate")p["posRot"]["pos"]["x"]=1e9;
  receive(p);require(sClients.at(2).pose.actor.world.pos.x==old,"invalid pose preserves usable state");
 }
 auto invalid=presence();invalid["name"]="mutated";invalid["curRoomNum"]="bad";
 receive({{"type","UPDATE_CLIENT_STATE"},{"clientId",2},{"state",invalid}});
 require(sClients.at(2).name=="friend","presence validates before mutation");
 auto valid=presence();valid["name"]="new";
 receive({{"type","ALL_CLIENT_STATE"},{"state",nlohmann::json::array({valid,invalid})}});
 require(sClients.at(2).name=="friend","roster stages whole packet before mutation");
 auto legacy=pose();legacy["type"]="DIPTYCH_PLAYER";legacy["form"]=legacy["transformation"];
 legacy["scene"]=legacy["sceneId"];legacy["room"]=legacy["roomIndex"];legacy["pos"]=legacy["posRot"]["pos"];
 legacy["rot"]=legacy["posRot"]["rot"];legacy["joints"]=legacy["jointTable"];legacy["upper"]=legacy["upperJointTable"];
 legacy["pos"]["x"]=99;receive(legacy);
 require(sClients.at(2).pose.actor.world.pos.x==99,"legacy receive normalization");
 peer=sClients.at(2).actor;receive(pose(1));peer->actor.update(&peer->actor,&play);frame();
 require(spawned==2 && sActors.size()==1,"form change waits for deferred destruction");
 cleanup();frame();require(spawned==3 && sClients.at(2).actor->transformation==1,"new form spawns after destroy");
 peer=sClients.at(2).actor;sClients.at(2).lastPose-=std::chrono::seconds(3);peer->actor.update(&peer->actor,&play);
 require(peer->actor.killed && !Visible(sClients.at(2),&play),"expired pose hides actor");cleanup();
 receive(pose());frame();require(spawned==4,"fresh pose restores expired peer");
 Suspend();require(local.actor.draw==PlayerCall_Draw && !sClients.at(2).hasPose,"park restores draw and invalidates pose");
 require(published.back()["isSaveLoaded"]==false,"park publishes unloaded state");
 receive(pose());Resume();frame();require(spawned==4,"parked and stale poses do not spawn on resume");cleanup();
 receive(pose());frame();require(spawned==5,"fresh resumed pose spawns");
 GameInteractor::OnRoomInit::hook(3,1);frame();require(spawned==5 && !sClients.at(2).hasPose,"room transition invalidates same-room pose");cleanup();
 receive(pose());frame();GameInteractor::OnSaveLoad::hook(1);frame();
 require(!sClients.at(2).hasPose,"save load invalidates pose");cleanup();
 receive(pose());frame();++offered.generation;frame();
 require(sClients.empty() && local.actor.draw==PlayerCall_Draw,"same-ID reconnect clears roster and draw");cleanup();
 roster();receive(pose());frame();offered.connected=false;frame();
 require(sClients.empty() && !sOwnColor && local.actor.draw==PlayerCall_Draw,"disconnect restores native preference");cleanup();
 offered.connected=true;frame();roster();auto combo=presence();combo.erase("diptych");
 receive({{"type","UPDATE_CLIENT_STATE"},{"clientId",2},{"state",combo}});receive(pose());frame();
 require(sClients.at(2).mm && sClients.at(2).hasPose,"native namespaced MM roster interoperates");
 auto oot=presence();oot["diptych"]["game"]="oot";receive({{"type","UPDATE_CLIENT_STATE"},{"clientId",2},{"state",oot}});
 receive(pose());require(!sClients.at(2).hasPose,"OoT roster cannot consume MM pose");cleanup();
 roster();failAllocation=true;receive(pose());frame();cleanup();failAllocation=false;
 require(colliderDestroys==destroyed*5,"initialized colliders cleaned even allocation fails");
 cleanup(true);GameInteractor::OnPlayDestroy::hook();gPlayState=nullptr;
 Shutdown();require(sActors.empty() && allocated.empty(),"scene teardown leaves no dangling actor ownership");
 std::cout<<assertions<<" native peer decoder/session/lifecycle assertions passed (game/render shells)"<<std::endl;
 return 0;
 } catch(const std::exception& error) { std::cerr<<error.what()<<std::endl;return 1; }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--json-include", required=True)
    args = parser.parse_args()
    native = (ROOT / "mm/2s2h/Network/Anchor/Peers.cpp").read_text()
    native = re.sub(r'^#include .*\n', '', native, flags=re.MULTILINE)
    start = native.index('extern "C" {')
    end = native.index('\n}\n', start) + 3
    native = native[:start] + native[end:]
    with tempfile.TemporaryDirectory(prefix="mm-peers-") as directory:
        temp = pathlib.Path(directory)
        (temp / "color.h").write_text('#pragma once\n#include <cstdint>\nstruct Color_RGBA8 { uint8_t r,g,b,a; };\n')
        (temp / "ultra64.h").write_text('#pragma once\n')
        cpp = temp / "probe.cpp"
        reset = function("mm/2s2h/Network/Anchor/Anchor.cpp", "void Anchor::ResetProtocol(")
        reset = reset.replace("Anchor::ResetProtocol", "ProtocolFixture::ResetProtocol")
        cpp.write_text(FIXTURE + native + CASES.replace("PROTOCOL_BODY", reset))
        exe = temp / "probe.exe"
        includes = [str(temp), str(ROOT / "mm/2s2h/Network/Anchor"), str(ROOT / "mm/2s2h/BenGui"), args.json_include]
        if pathlib.Path(args.cxx).name.lower() in {"cl", "cl.exe"}:
            command = [args.cxx, "/nologo", "/std:c++20", "/EHsc", "/W4", "/DENABLE_ANCHOR", str(cpp),
                       *["/I" + path for path in includes], "/Fe:" + str(exe)]
        else:
            command = [args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                       "-Wno-missing-field-initializers", "-DENABLE_ANCHOR", str(cpp),
                       *["-I" + path for path in includes], "-o", str(exe)]
        subprocess.run(command, check=True, cwd=temp)
        subprocess.run([str(exe)], check=True, cwd=temp)


if __name__ == "__main__":
    main()
