"""Exercise actual owl activation/access bodies and reward flag lookup with game services stubbed.
Run: python tests/test_owl_access.py --cxx g++. No game, assets or server is launched.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def function(text, name):
    start = text.index(name + "(")
    start = text.rfind(chr(10), 0, start) + 1
    brace = text.index("{", start)
    depth, end = 1, brace + 1
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
    strip = lambda s: re.sub(r"^#include.*$", "", s, flags=re.M)
    gi = read("mm/2s2h/GameInteractor/GameInteractor.h")
    flag_enum = gi[gi.index("typedef enum {"):gi.index("} FlagType;") + len("} FlagType;")]
    checks = read("mm/2s2h/Rando/StaticData/Checks.cpp")
    # Preserve every actual check's flag type. Owl lookup must reject all of them before consulting flag/scene.
    types = re.findall(r"RC\(RC_\w+,\s*RCTYPE_\w+,\s*SCENE_\w+,\s*(FLAG_\w+)", checks)
    assert types and "FLAG_OWL_ACTIVATION" not in types
    catalog = ",".join("{" + str(i + 1) + ",{ " + str(i + 1) + "," + t + ",0,SCENE_MAX}}" for i, t in enumerate(types))
    fixture = r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
using u8=uint8_t;using u32=uint32_t;using s16=int16_t;using s32=int32_t;
constexpr int SCENE_MAX=113, SCENE_INISIE_R=112, RC_UNKNOWN=0, OWL_WARP_NONE=255;
FLAG_ENUM
struct {struct {struct {struct {uint16_t owlActivationFlags=0x8000;u8 owlWarpId=OWL_WARP_NONE;} playerData;} saveInfo;} save;} gSaveContext;
#define SET_OWL_STATUE_ACTIVATED(id) (gSaveContext.save.saveInfo.playerData.owlActivationFlags |= (1u << (id)))
namespace AnchorProgress {
enum class ApplyResult {Rejected,NotReady,Unknown,Applied};
bool loaded=true,ready=true;
bool LocalReady(){return loaded;}
bool Ready(){return ready;}
}
struct Check {int randoCheckId;FlagType flagType;s32 flag;s16 sceneId;};
using RandoStaticCheck=Check;
s16 Play_GetOriginalSceneId(s16 scene){return scene;}
namespace Rando::StaticData {
std::map<int,Check> Checks={{0,{RC_UNKNOWN,FLAG_NONE,0,SCENE_MAX}},CATALOG};
RandoStaticCheck GetCheckFromFlag(FlagType,s32,s16=SCENE_MAX);
LOOKUP
}
struct SaveCheck {bool shuffled=true,eligible=false,obtained=false,cycleObtained=false;};
std::map<int,SaveCheck> saveChecks;
#define RANDO_SAVE_CHECKS saveChecks
namespace Rando::MiscBehavior {
ON_FLAG
}
struct GameInteractor {
 struct OnFlagSet {using fn=std::function<void(FlagType,u32)>;};
 static GameInteractor* Instance; OnFlagSet::fn hook; int registrations=0;
 template<class H>void RegisterGameHook(typename H::fn callback){hook=callback;++registrations;}
} gi;
GameInteractor* GameInteractor::Instance=&gi;
int earnedEvents=0;
void GameInteractor_ExecuteOnFlagSet(FlagType type,u32 flag){
 ++earnedEvents; Rando::MiscBehavior::OnFlagSet(type,flag); if(gi.hook)gi.hook(type,flag);
}
ACTIVATE
HEADER
BODY
int outbound=0;uint16_t sent=0;
void Send(uint16_t mask){++outbound;sent|=mask;}
int main(){
 using AnchorProgress::ApplyResult;
 OwlAccess::Register(Send); OwlAccess::Register(Send);assert(gi.registrations==1);
 Sram_ActivateOwl(9);assert(earnedEvents==1&&outbound==1&&sent==512);
 assert(gSaveContext.save.saveInfo.playerData.owlWarpId==9);
 Sram_ActivateOwl(9);assert(earnedEvents==1&&outbound==1);
 Sram_ActivateOwl(10);Sram_ActivateOwl(255);assert(earnedEvents==1);
 assert(saveChecks.empty()); // Actual native lookup returns unknown; no shuffled check becomes eligible.
 OwlAccess::State captured;assert(OwlAccess::Capture(captured)&&captured.known&&captured.mask==512);
 AnchorProgress::ready=false;Sram_ActivateOwl(1);assert(outbound==2&&sent==514);
 assert(OwlAccess::Apply({true,1})==ApplyResult::NotReady);
 AnchorProgress::ready=true;assert(OwlAccess::Apply({false,0})==ApplyResult::Unknown);
 assert(OwlAccess::Apply({true,0x8000})==ApplyResult::Rejected);
 const int before=earnedEvents;assert(OwlAccess::Apply({true,5})==ApplyResult::Applied);
 assert(OwlAccess::Apply({true,0})==ApplyResult::Applied);
 assert(gSaveContext.save.saveInfo.playerData.owlActivationFlags==(0x8000|512|2|5));
 assert(gSaveContext.save.saveInfo.playerData.owlWarpId==9&&earnedEvents==before&&saveChecks.empty());
 AnchorProgress::loaded=false;captured={};assert(!OwlAccess::Capture(captured)&&!captured.known);
 AnchorProgress::loaded=true;OwlAccess::Register(nullptr);Sram_ActivateOwl(3);assert(outbound==2);
 OwlAccess::Register(Send);assert(gi.registrations==1);
 std::cout<<"Native owl earned-access and remote union regression passed"<<std::endl;
}
'''
    replacements = {
        "FLAG_ENUM": flag_enum,
        "LOOKUP": function(checks, "GetCheckFromFlag"),
        "ON_FLAG": function(read("mm/2s2h/Rando/MiscBehavior/OnFlagSet.cpp"), "Rando::MiscBehavior::OnFlagSet").replace("Rando::MiscBehavior::", ""),
        "ACTIVATE": function(read("mm/src/code/z_sram_NES.c"), "Sram_ActivateOwl"),
        "HEADER": strip(read("mm/2s2h/Network/Anchor/OwlAccess.h")),
        "BODY": strip(read("mm/2s2h/Network/Anchor/OwlAccess.cpp")),
    }
    fixture = fixture.replace("CATALOG", catalog)
    for key, value in replacements.items():
        fixture = fixture.replace(chr(10) + key + chr(10), chr(10) + value + chr(10))
    with tempfile.TemporaryDirectory(prefix="mm-owl-access-") as temp:
        source, output = Path(temp) / "test.cpp", Path(temp) / "test"
        source.write_text(fixture)
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)


if __name__ == "__main__":
    main()
