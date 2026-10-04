"""Exercise the native progress component with actual reset/check data and stubbed game services.
Run: python tests/test_permanent_progress.py --cxx g++
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def function(text, name):
    start = text.index(name + "(")
    start = text.rfind("\n", 0, start) + 1
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    read = lambda p: (root / p).read_text()
    table = read("mm/include/tables/scene_table.h")
    scenes = re.findall(r"/\* (0x[0-9A-Fa-f]+) \*/ DEFINE_SCENE(?:_UNSET)?\((?:[^,]+,\s*)?(SCENE_\w+)", table)
    scene_count = re.search(r"/\* (0x[0-9A-Fa-f]+) \*/ SCENE_MAX", read("mm/include/z64scene.h")).group(1)
    enum = "enum {" + ",".join(name + "=" + num for num, name in scenes) + ",SCENE_MAX=" + scene_count + "};"
    checks = []
    for line in read("mm/2s2h/Rando/StaticData/Checks.cpp").splitlines():
        if "FLAG_CYCL_SCENE_SWITCH" in line:
            fields = [s.strip() for s in line.split(",")]
            checks.append("{" + str(len(checks)) + ",{" + fields[2] + ",FLAG_CYCL_SCENE_SWITCH," + fields[4] + "}}")
    header = read("mm/2s2h/Network/Anchor/PermanentProgress.h")
    body = read("mm/2s2h/Network/Anchor/PermanentProgress.cpp")
    header = re.sub(r"^#include.*$", "", header, flags=re.M)
    body = re.sub(r"^#include.*$", "", body, flags=re.M)
    native = "\n".join([function(read("mm/src/code/z_play.c"), "Play_GetOriginalSceneId"),
                         function(read("mm/src/code/z_sram_NES.c"), "Sram_GetPersistentCycleSwitchMask"),
                         function(read("mm/src/code/z_actor.c"), "Flags_GetSwitch"),
                         function(read("mm/src/code/z_actor.c"), "Flags_SetSwitch"),
                         function(read("mm/src/code/z_actor.c"), "Flags_UnsetSwitch")])
    fixture = r'''#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <type_traits>
#include <iostream>
using u32=uint32_t;using u8=uint8_t;using s16=int16_t;using s32=int32_t;
ENUM
constexpr int GAMEMODE_NORMAL=0,TRANS_TRIGGER_OFF=0,TRANS_MODE_OFF=0,SWITCH_FLAG_NONE=-1;
enum FlagType { FLAG_NONE,FLAG_CYCL_SCENE_SWITCH };
struct Bank {u32 switch0=0,switch1=0;};
struct SaveContext {int gameMode=0,fileNum=0;Bank cycleSceneFlags[SCENE_MAX];struct {struct {Bank permanentSceneFlags[SCENE_MAX];} saveInfo;} save;} gSaveContext;
int playerDummy=1;
struct PlayState {s16 sceneId=SCENE_ROMANYMAE;struct {bool running=true;} state;int* player=&playerDummy;int transitionTrigger=0,transitionMode=0;struct {int status=0;} sramCtx;struct {struct {u32 switches[4]={};} sceneFlags;} actorCtx;} play;
PlayState* gPlayState=&play;
#define GET_PLAYER(p) ((p)->player)
struct PersistentCycleSceneFlags {u32 switch0,switch1,chest,collectible;};
#define PERSISTENT_CYCLE_FLAGS_SET(a,b,c,d) {u32(a),u32(b),u32(c),u32(d)}
#define PERSISTENT_CYCLE_FLAGS_NONE PERSISTENT_CYCLE_FLAGS_SET(0,0,0,0)
#define DEFINE_SCENE(a,b,c,d,e,flags,g,h,i) flags,
#define DEFINE_SCENE_UNSET(a) PERSISTENT_CYCLE_FLAGS_NONE,
PersistentCycleSceneFlags sPersistentCycleSceneFlags[]={
TABLE
};
#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET
namespace Rando::StaticData {struct Check {s16 sceneId;FlagType flagType;int flag;};std::map<int,Check> Checks={CHECKS};}
struct GameInteractor {
 struct OnSceneFlagSet {using fn=std::function<void(s16,FlagType,u32)>;};struct OnSceneFlagUnset {using fn=OnSceneFlagSet::fn;};
 OnSceneFlagSet::fn set,unset;int registrations=0;static GameInteractor* Instance;
 template<class H> void RegisterGameHook(typename H::fn fn) {++registrations;if constexpr(std::is_same_v<H,OnSceneFlagSet>)set=fn;else unset=fn;}
} gi;
GameInteractor* GameInteractor::Instance=&gi;
int nativeEvents=0;
void GameInteractor_ExecuteOnSceneFlagSet(s16 scene,FlagType type,u32 flag){++nativeEvents;if(gi.set)gi.set(scene,type,flag);}
void GameInteractor_ExecuteOnSceneFlagUnset(s16 scene,FlagType type,u32 flag){++nativeEvents;if(gi.unset)gi.unset(scene,type,flag);}
NATIVE
HEADER
BODY
int outgoing=0;
void Send(const AnchorProgress::Edit& e){assert(e.scene==SCENE_ROMANYMAE);++outgoing;}
int main(){
 using namespace AnchorProgress;
 assert(CanonicalScene(SCENE_INISIE_R)==SCENE_INISIE_N);assert(CanonicalScene(SCENE_20SICHITAI2)==SCENE_20SICHITAI);assert(CanonicalScene(-1)==-1);
 assert(EligibleMask(SCENE_ROMANYMAE,0)==(1u<<10));assert(EligibleMask(SCENE_TAKARAYA,0)==0);assert(EligibleMask(SCENE_SEA,1)==0);
 for(const auto& [id,c]:Rando::StaticData::Checks)assert((EligibleMask(c.sceneId,c.flag/32)&(1u<<(c.flag%32)))==0);
 Register(Send);Register(Send);assert(gi.registrations==2);
 Flags_SetSwitch(&play,10);Flags_SetSwitch(&play,10);Flags_UnsetSwitch(&play,10);assert(outgoing==2);
 play.sramCtx.status=7;Flags_SetSwitch(&play,10);Flags_UnsetSwitch(&play,10);assert(outgoing==4);
 State busy;assert(Capture(busy)&&busy.switches[SCENE_ROMANYMAE][0]==0);assert(Apply(Edit{SCENE_ROMANYMAE,10,true})==ApplyResult::NotReady);play.sramCtx.status=0;
 gSaveContext.cycleSceneFlags[SCENE_ROMANYMAE].switch0=(1u<<10)|(1u<<2);
 gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_ROMANYMAE].switch0=(1u<<10)|(1u<<3);
 State state;assert(Capture(state));assert(state.known&&state.switches[SCENE_ROMANYMAE][0]==0);
 int before=nativeEvents;State unknown;assert(Apply(unknown)==ApplyResult::Unknown);assert(nativeEvents==before);
 assert(Apply(Edit{SCENE_TAKARAYA,1,true})==ApplyResult::Rejected);
 assert(Apply(Edit{SCENE_ROMANYMAE,64,true})==ApplyResult::Rejected);
 assert(Apply(Edit{SCENE_ROMANYMAE,10,true})==ApplyResult::Applied);
 assert(Apply(Edit{SCENE_ROMANYMAE,10,false})==ApplyResult::Applied);
 assert(Apply(Edit{SCENE_ROMANYMAE,10,true})==ApplyResult::Applied);assert(outgoing==4&&!IsApplyingRemote());
 State empty;empty.known=true;assert(Apply(empty)==ApplyResult::Applied);
 assert(play.actorCtx.sceneFlags.switches[0]==0);assert(gSaveContext.cycleSceneFlags[SCENE_ROMANYMAE].switch0==(1u<<2));assert(gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_ROMANYMAE].switch0==(1u<<3));
 State invalid=empty;invalid.switches[SCENE_TAKARAYA][0]=2;assert(Apply(invalid)==ApplyResult::Rejected);
 play.transitionTrigger=1;state.known=false;assert(!Capture(state)&&!state.known);assert(Apply(empty)==ApplyResult::NotReady);play.transitionTrigger=0;
 play.state.running=false;assert(!Ready());play.state.running=true;gPlayState=nullptr;assert(!Ready());gPlayState=&play;
 assert(Apply(Edit{SCENE_INISIE_R,26,true})==ApplyResult::Applied);assert(gSaveContext.cycleSceneFlags[SCENE_INISIE_N].switch0==(1u<<26));assert(gSaveContext.cycleSceneFlags[SCENE_INISIE_R].switch0==0);
 Register(nullptr);Flags_SetSwitch(&play,10);assert(outgoing==4);Register(Send);assert(gi.registrations==2);
 gi.set=[](s16,FlagType,u32){throw 7;};Flags_UnsetSwitch(&play,10);
 try{Apply(Edit{SCENE_ROMANYMAE,10,true});assert(false);}catch(int){assert(!IsApplyingRemote());}
 std::cout<<"Permanent switch eligibility, capture and apply regression passed"<<std::endl;
}
'''
    fixture = fixture.replace("ENUM", enum).replace("TABLE", table).replace("CHECKS", ",".join(checks))
    fixture = fixture.replace("NATIVE", native).replace("HEADER", header).replace("BODY", body)
    with tempfile.TemporaryDirectory(prefix="mm-permanent-progress-") as temp:
        source = Path(temp) / "test.cpp"
        output = Path(temp) / "test"
        source.write_text(fixture)
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)


if __name__ == "__main__":
    main()
