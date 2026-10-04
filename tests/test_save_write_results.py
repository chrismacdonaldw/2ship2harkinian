"""Compile executed native flash/SRAM bodies with disposable file and game-service fixtures.

Run: python tests/test_save_write_results.py --cxx g++ --json-include <include>
The extracted bodies are not snapshots; changed native code is exercised on every run.
This covers result flow, not a full port build or gameplay presentation.
"""
import argparse
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def function(path, signature, port_only=False):
    text = (ROOT / path).read_text()
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    body = text[start:end]
    if port_only:
        body = body[:body.index("// #endregion")] + "}"
    return body


def source():
    cpp = FIXTURE
    for name in ["static s32 WriteFlashData(", 'extern "C" s32 SaveManager_SysFlashrom_WriteData(']:
        cpp += function("mm/2s2h/SaveManager/SaveManager.cpp", name) + '\n'
    cpp += "struct { s32 response; } sFlashromRequest;\n"
    for name in ["void SysFlashrom_WriteDataAsync(", "s32 SysFlashrom_AwaitResult("]:
        cpp += function("mm/src/code/sys_flashrom.c", name, True) + '\n'
    for name in ["void Sram_SetFlashPagesDefault(", "void Sram_StartWriteToFlashDefault(",
                 "void Sram_UpdateWriteToFlashDefault(", "void Sram_SetFlashPagesOwlSave(",
                 "void Sram_StartWriteToFlashOwlSave(", "void Sram_UpdateWriteToFlashOwlSave("]:
        cpp += function("mm/src/code/z_sram_NES.c", name) + '\n'
    text = (ROOT / "mm/src/code/z_message.c").read_text()
    start = text.index("        case MSGMODE_OWL_SAVE_1:", text.index("void Message_Update"))
    end = text.index("        case MSGMODE_9:", start)
    cpp += OWL_FIXTURE + "void AwaitOwl(PlayState* play) { auto* msgCtx=&play->msgCtx; switch(msgCtx->msgMode) {" + text[start:end] + "}}"
    text = (ROOT / "mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c").read_text()
    start = text.index("                case PAUSE_SAVEPROMPT_STATE_4:", text.index("void KaleidoScope_Update"))
    end = text.index("                case PAUSE_SAVEPROMPT_STATE_5:", start)
    cpp += "void AwaitPause(SramContext* sramCtx, PauseContext* pauseCtx) { switch(pauseCtx->savePromptState) {" + text[start:end] + "}}"
    return cpp + CASES


FIXTURE = r'''#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include "SaveFile.h"
using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t; using s16 = int16_t; using s32 = int32_t;
using OSTime = uint64_t;
#define DIPTYCH_GAME_MODULE
static uint64_t sDiptychFlashWrites[14]{};
namespace DiptychGoals { bool CycleSavePending() { return false; } }
#define SPDLOG_ERROR(...) ((void)0)
namespace Notification { struct Options { std::string message; }; int count = 0; bool fail = false; void Emit(Options) { if (fail) throw std::runtime_error("notification fixture"); ++count; } }
struct Save { bool valid = true; bool isOwlSave = false; int cutsceneIndex=0; struct { u16 checksum = 0; } saveInfo; };
struct SaveContext { Save save; int progress = 123; int fileNum = 0; int gameMode=0, sceneLayer=0; } gSaveContext;
struct SaveOptions {};
void to_json(nlohmann::json& j, const Save&) { j = { {"valid", true} }; }
void to_json(nlohmann::json& j, const SaveContext& s) { j = { {"save", s.save}, {"progress", s.progress} }; }
#define IS_VALID_FILE(s) ((s).valid)
enum FlashSave { FLASH_SAVE_SRAM_HEADER, FLASH_SAVE_SRAM_HEADER_BACKUP,
 FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE, FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP,
 FLASH_SAVE_FILE_1_OWL_SAVE, FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP,
 FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE, FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP,
 FLASH_SAVE_FILE_2_OWL_SAVE, FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP,
 FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE, FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP,
 FLASH_SAVE_FILE_3_OWL_SAVE, FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP };
#define FLASH_SAVE_UNAVAILABLE ((FlashSave)-1)
u32 gFlashSaveStartPages[14] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13};
u32 gFlashSaveNumPages[14] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1};
u32 gFlashSpecialSaveNumPages[14] = {2,2,2,2,2,2,2,2,2,2,2,2,2,2};
const int CURRENT_SAVE_VERSION = 8;
std::filesystem::path root;
int readFailure = 0, migrateFailure = 0, writes = 0, deletes = 0, reads = 0;
bool throwWrite = false, failReadback = false;
std::string failedFile;
FlashSave SaveManager_GetFlashSaveFromPages(u32 p, u32) {
 if (p >= 128) p = p - 128 + 1;
 return p < 14 ? (FlashSave)p : FLASH_SAVE_UNAVAILABLE;
}
std::string SaveManager_GetFileNameFromFlashSave(FlashSave f) { return std::to_string((int)f) + ".json"; }
void SaveManager_WriteGlobalOptions(SaveOptions) {}
int SaveManager_ReadSaveFile(const std::string& f, nlohmann::json& j) {
 if (readFailure) return readFailure;
 std::ifstream in(root / f); if (!in.is_open()) return -1;
 in >> j; return 0;
}
int SaveManager_MigrateSave(nlohmann::json&) { return migrateFailure; }
bool SaveManager_WriteSaveFile(const std::string& f, nlohmann::json j) {
 ++writes; if (throwWrite) throw std::runtime_error("write fixture");
 if (f == failedFile) return false;
 return MmSaveFile::Publish(root / f, j.dump());
}
void SaveManager_DeleteSaveFile(const std::string&) { ++deletes; }
struct SramContext { u8* saveBuf; s16 status = 0; u32 curPage = 0, numPages = 0;
 OSTime startWriteOsTime = 0; s32 writeResult = 0; s16 owlSaveBeforeWrite=0; u16 checksumBeforeWrite=0; };
OSTime osGetTime() { return 100; }
int SysFlashrom_IsBusy() { return 1; }
#define VB_SAVE_DELAY 0
#define OSTIME_TO_TIMER(x) (x)
#define SECONDS_TO_TIMER(x) (x)
#define GameInteractor_Should(a,b) true
#define SAVE_BUFFER_SIZE 256
s32 SysFlashrom_ReadData(void* p, u32, u32) {
 ++reads; if (failReadback) return -1; memcpy(p, &gSaveContext, offsetof(SaveContext,fileNum)); return 0;
}
'''

OWL_FIXTURE = r'''enum { MSGMODE_OWL_SAVE_1=1, MSGMODE_OWL_SAVE_2, OCARINA_MODE_APPLY_SOT,
 GAMEMODE_OWL_SAVE, TRANS_TRIGGER_START, TRANS_TYPE_FADE_BLACK, CUTSCENE,
 PAUSE_SAVEPROMPT_STATE_4, PAUSE_SAVEPROMPT_STATE_5, PAUSE_SAVEPROMPT_STATE_1 };
#define ENTRANCE(a,b) 10
struct MessageContext { int msgMode=MSGMODE_OWL_SAVE_1, ocarinaMode=0; };
struct PlayState { SramContext sramCtx; MessageContext msgCtx; struct { int unk_A3=1; } state;
 int transitionTrigger=0, transitionType=0, nextEntrance=0; };
struct PauseContext { int savePromptState=PAUSE_SAVEPROMPT_STATE_4; };
void Message_CloseTextbox(PlayState* p) { p->msgCtx.msgMode=0; }
'''

CASES = r'''
void require(bool p, const char* name) { if (!p) throw std::runtime_error(name); }
void reset() { readFailure = migrateFailure = writes = deletes = reads = 0;
 throwWrite = failReadback = false; failedFile.clear(); Notification::count = 0; Notification::fail=false;
 memset(sDiptychFlashWrites, 0, sizeof(sDiptychFlashWrites)); }
std::string bytes(const char* f) { std::ifstream in(root/f); return std::string(std::istreambuf_iterator<char>(in), {}); }
void previous(const char* f) { std::ofstream(root/f) << R"({"newCycleSave":{"old":1},"owlSave":{"old":2}})"; }
int main(int argc, char** argv) {
 try {
 root = argv[1]; std::filesystem::create_directory(root);
 SaveContext live; u8 buffer[SAVE_BUFFER_SIZE]{}; memcpy(buffer,&live,offsetof(SaveContext,fileNum));
 previous("2.json"); const auto old = bytes("2.json");
 reset(); failedFile="2.json";
 require(SaveManager_SysFlashrom_WriteData(buffer,2,1)==-1,"publication failure propagates");
 require(bytes("2.json")==old && sDiptychFlashWrites[2]==0,"last good file and serial preserved");
 require(Notification::count==1,"one failure notification");
 reset(); readFailure=-2;
 require(SaveManager_SysFlashrom_WriteData(buffer,2,1)==-1 && writes==0 && deletes==0,"read failure refuses publication");
 require(bytes("2.json")==old,"read failure preserves destination");
 reset(); migrateFailure=-1;
 require(SaveManager_SysFlashrom_WriteData(buffer,2,1)==-1 && writes==0 && deletes==0,"migration failure refuses publication");
 reset(); throwWrite=true;
 require(SaveManager_SysFlashrom_WriteData(buffer,2,1)==-1,"C bridge catches writer exceptions");
 reset(); failedFile="2.json"; Notification::fail=true;
 require(SaveManager_SysFlashrom_WriteData(buffer,2,1)==-1,"reporting failure remains inside C boundary");
 reset(); failedFile="3.json";
 require(SaveManager_SysFlashrom_WriteData(buffer,2,2)==-1,"special backup failure aggregates");
 require(sDiptychFlashWrites[2]==1 && sDiptychFlashWrites[3]==0 && Notification::count==1,"successful primary retained without false backup serial");
 reset();
 require(SaveManager_SysFlashrom_WriteData(buffer,2,2)==0 && sDiptychFlashWrites[2]==1 && sDiptychFlashWrites[3]==1,"special retry succeeds");
 auto j=nlohmann::json::parse(bytes("2.json")); require(j.contains("owlSave"),"normal write preserves opposite half");
 reset(); previous("4.json"); failedFile="4.json"; live.save.valid=false;
 memcpy(buffer,&live,offsetof(SaveContext,fileNum)); const auto owlOld=bytes("4.json");
 require(SaveManager_SysFlashrom_WriteData(buffer,4,1)==-1 && bytes("4.json")==owlOld,"owl removal rewrite failure preserves file");
 live.save.valid=true; memcpy(buffer,&live,offsetof(SaveContext,fileNum));
 SramContext ctx; ctx.saveBuf=buffer;
 reset(); failedFile="4.json"; Sram_SetFlashPagesOwlSave(&ctx,4,1); Sram_StartWriteToFlashOwlSave(&ctx);
 sFlashromRequest.response=0;
 Sram_UpdateWriteToFlashDefault(&ctx);
 require(ctx.status==0 && ctx.writeResult==-1 && reads==0,"pause/autosave default route sees owl write failure");
 reset(); failedFile="4.json"; Sram_SetFlashPagesOwlSave(&ctx,4,1); Sram_StartWriteToFlashOwlSave(&ctx);
 Sram_UpdateWriteToFlashOwlSave(&ctx);
 require(ctx.status==0 && ctx.writeResult==-1 && writes==1 && reads==0,"owl primary failure skips backup/readback");
 reset(); failedFile="5.json"; Sram_SetFlashPagesOwlSave(&ctx,4,1); Sram_StartWriteToFlashOwlSave(&ctx);
 Sram_UpdateWriteToFlashOwlSave(&ctx); Sram_UpdateWriteToFlashOwlSave(&ctx);
 require(ctx.status==0 && ctx.writeResult==-1 && reads==0,"owl backup failure skips readback");
 reset(); Sram_SetFlashPagesOwlSave(&ctx,4,1); Sram_StartWriteToFlashOwlSave(&ctx);
 Sram_UpdateWriteToFlashOwlSave(&ctx); sFlashromRequest.response=-1;
 Sram_UpdateWriteToFlashOwlSave(&ctx); Sram_UpdateWriteToFlashOwlSave(&ctx);
 require(ctx.status==0 && ctx.writeResult==0 && reads==1,"owl retry completes normally");
 reset(); failReadback=true; gSaveContext.progress=456;
 Sram_SetFlashPagesOwlSave(&ctx,4,1); Sram_StartWriteToFlashOwlSave(&ctx);
 Sram_UpdateWriteToFlashOwlSave(&ctx); Sram_UpdateWriteToFlashOwlSave(&ctx); Sram_UpdateWriteToFlashOwlSave(&ctx);
 require(ctx.status==0 && ctx.writeResult==-1 && gSaveContext.progress==456,"failed readback preserves live progress");
 memcpy(buffer,&live,offsetof(SaveContext,fileNum));
 reset(); failedFile="2.json"; Sram_SetFlashPagesDefault(&ctx,2,1); Sram_StartWriteToFlashDefault(&ctx);
 Sram_UpdateWriteToFlashDefault(&ctx); require(ctx.status==0 && ctx.writeResult==-1,"default failure completes finitely");
 reset(); Sram_SetFlashPagesDefault(&ctx,2,1); Sram_StartWriteToFlashDefault(&ctx);
 sFlashromRequest.response=-1;
 Sram_UpdateWriteToFlashDefault(&ctx); Sram_UpdateWriteToFlashDefault(&ctx);
 require(ctx.status==0 && ctx.writeResult==0,"owned operation result survives unrelated failed response");
 PlayState play; play.sramCtx.status=0; play.sramCtx.writeResult=-1;
 play.sramCtx.owlSaveBeforeWrite=true; play.sramCtx.checksumBeforeWrite=77;
 gSaveContext.fileNum=0; gSaveContext.save.isOwlSave=false; gSaveContext.save.saveInfo.checksum=9;
 AwaitOwl(&play);
 require(play.msgCtx.msgMode==0 && play.state.unk_A3==0 && play.transitionTrigger==0,"failed owl closes prompt without quitting");
 require(gSaveContext.save.isOwlSave && gSaveContext.save.saveInfo.checksum==77,"failed owl restores prior prepared controls");
 PauseContext pause; AwaitPause(&play.sramCtx,&pause);
 require(pause.savePromptState==PAUSE_SAVEPROMPT_STATE_1,"failed pause save returns to retry choice");
 pause.savePromptState=PAUSE_SAVEPROMPT_STATE_4; play.sramCtx.writeResult=0; AwaitPause(&play.sramCtx,&pause);
 require(pause.savePromptState==PAUSE_SAVEPROMPT_STATE_5,"successful pause save retains saved message");
 play.msgCtx.msgMode=MSGMODE_OWL_SAVE_1; AwaitOwl(&play);
 require(play.transitionTrigger==TRANS_TRIGGER_START && play.msgCtx.msgMode==MSGMODE_OWL_SAVE_2,"successful owl retains native quit path");
 std::cout << "Native flash and SRAM write-result scenarios passed\n";
 return 0;
 } catch(const std::exception& e) { std::cerr << e.what() << "\n"; return 1; }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--json-include", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mm-write-results-") as tmp:
        tmp = pathlib.Path(tmp)
        cpp = tmp / "probe.cpp"
        cpp.write_text(source())
        exe = tmp / "probe.exe"
        include = ROOT / "mm/2s2h/SaveManager"
        if pathlib.Path(args.cxx).name.lower() in {"cl", "cl.exe"}:
            cmd = [args.cxx, "/nologo", "/std:c++20", "/EHsc", "/W4", str(cpp),
                   "/I" + str(include), "/I" + args.json_include, "/Fe:" + str(exe)]
        else:
            cmd = [args.cxx, "-std=c++20", "-pthread", str(cpp), "-I" + str(include),
                   "-I" + args.json_include, "-o", str(exe)]
        subprocess.run(cmd, cwd=tmp, check=True)
        subprocess.run([str(exe), str(tmp / "saves")], check=True)


if __name__ == "__main__":
    main()
