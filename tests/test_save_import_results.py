"""Run the actual JSON/binary import handlers and desktop publisher with disposable files.

Run: python tests/test_save_import_results.py --json-include <include>
Native conversion, file-select and notification services are fixture shells, not a port build.
"""
import argparse
import pathlib
import subprocess
import tempfile

from test_save_write_results import function

ROOT = pathlib.Path(__file__).resolve().parents[1]


def source():
    manager = (ROOT / "mm/2s2h/SaveManager/SaveManager.cpp").read_text()
    binary = (ROOT / "mm/2s2h/SaveManager/BinarySaveConverter.cpp").read_text()
    json_handler = manager[manager.index("bool SaveManager_HandleFileDropped("):
                           manager.index("#define SAVE_OPTIONS_VALID_ID")]
    binary_handler = binary[binary.index("bool BinarySaveConverter_HandleFileDropped("):]
    return (FIXTURE + function("mm/2s2h/SaveManager/SaveManager.cpp", "bool SaveManager_WriteSaveFile(")
            + json_handler + binary_handler + CASES)


FIXTURE = r'''#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include "SaveFile.h"
std::filesystem::path savesFolderPath;
int openSlot = 1, stops = 0, starts = 0;
std::vector<std::string> errors, successes;
namespace Notification {
struct Options { std::string message; std::string suffix = {}; };
std::vector<Options> messages;
void Emit(Options options) { messages.push_back(options); }
}
#define SPDLOG_ERROR(message, ...) errors.push_back(message)
#define SPDLOG_INFO(message, ...) successes.push_back(message)
#define SPDLOG_DEBUG(...) ((void)0)
struct FileSelect { int state; } fileSelect{};
FileSelect* gFileSelectState = &fileSelect;
#define STOP_GAMESTATE(...) (++stops)
#define SET_NEXT_GAMESTATE(...) (++starts)
int SaveManager_GetOpenFileSlot() { return openSlot; }
std::string SaveManager_GetFileName(int slot) { return "file" + std::to_string(slot) + ".json"; }
struct Legacy_Save { int marker; };
struct Legacy_SaveContext { Legacy_Save save; };
void to_json(nlohmann::json& j, const Legacy_Save& save) { j = {{"marker", save.marker}}; }
namespace Ship {
enum class Endianness { Big }; enum class SeekOffsetType { Start };
struct BinaryReader {
 BinaryReader(const char*, size_t) {}
 void SetEndianness(Endianness) {}
 void Seek(size_t, SeekOffsetType) {}
};
}
void BinarySaveConverter_ReadBufferToSave(Legacy_SaveContext* save, std::shared_ptr<Ship::BinaryReader>) {
 save->save.marker = 42;
}
void to_json(nlohmann::json& j, const Legacy_SaveContext& save) { j = {{"save", {{"marker",save.save.marker}}}}; }
'''
CASES = r'''
void require(bool condition, const char* name) { if (!condition) throw std::runtime_error(name); }
std::string bytes(const std::filesystem::path& path) {
 std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}};
}
void reset() { stops = starts = 0; errors.clear(); successes.clear(); Notification::messages.clear(); }
int main(int argc, char** argv) {
 try {
 require(argc == 2, "disposable root required");
 const std::filesystem::path root = argv[1]; std::filesystem::create_directory(root);
 auto jsonPath = (root / "input.json").string();
 std::ofstream(jsonPath) << R"({"type":"2S2H_SAVE","newCycleSave":{"save":{"marker":7}}})";
 auto binaryPath = (root / "input.sra").string();
 std::string binary(512, char{0}); binary.replace(36,4,"ZELD");
 std::ofstream(binaryPath,std::ios::binary).write(binary.data(),binary.size());
 using Handler = bool(*)(char*);
 const Handler handlers[] = {SaveManager_HandleFileDropped, BinarySaveConverter_HandleFileDropped};
 std::string paths[] = {jsonPath, binaryPath};
 for (int i=0; i<2; ++i) {
  savesFolderPath = root / std::to_string(i); std::filesystem::create_directory(savesFolderPath);
  const auto destination = savesFolderPath / "file1.json";
  std::ofstream(destination) << "last-good";
  const auto old = bytes(destination);
  const auto first = MmSaveFile::NextTemporary();
  for (uint64_t n=first; n<first+64; ++n) std::filesystem::create_directory(destination.string()+".tmp-"+std::to_string(n));
  reset();
  require(handlers[i](paths[i].data()), "recognized failed import stays handled");
  require(bytes(destination)==old, "failed publication preserves destination");
  require(stops==0 && starts==0, "failed import does not refresh file select");
  require(successes.empty() && !errors.empty(), "failure logged without success");
  require(Notification::messages.size()==1 && Notification::messages[0].message=="Failed to import save into slot",
          "failure notification replaces success");
  require(Notification::messages[0].suffix=="1", "failure identifies requested slot");
  for (uint64_t n=first; n<first+64; ++n) {
   require(std::filesystem::is_directory(destination.string()+".tmp-"+std::to_string(n)),"publisher preserves foreign collisions");
   std::filesystem::remove(destination.string()+".tmp-"+std::to_string(n));
  }
  reset();
  require(handlers[i](paths[i].data()), "successful import handled");
  const auto imported = nlohmann::json::parse(bytes(destination));
  require(imported["type"]=="2S2H_SAVE" && imported["newCycleSave"]["save"]["marker"]==(i==0?7:42),
          "successful import publishes converted data");
  require(stops==1 && starts==1 && successes.size()==1 && errors.empty(), "success refresh and log retained");
  require(Notification::messages.size()==1 && Notification::messages[0].message=="Successfully imported save into slot",
          "success notification retained");
  openSlot=-1; reset();
  require(handlers[i](paths[i].data()) && stops==0 && starts==0 && successes.empty(), "no-slot remains handled without success");
  openSlot=1;
 }
 reset();
 require(!SaveManager_HandleFileDropped(binaryPath.data()) && !BinarySaveConverter_HandleFileDropped(jsonPath.data()),
         "unrecognized input remains available to other handlers");
 std::cout << "JSON and binary import failure, recovery, no-slot and routing scenarios passed" << std::endl;
 return 0;
 } catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--json-include", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mm-import-results-") as temporary:
        tmp = pathlib.Path(temporary)
        cpp = tmp / "probe.cpp"
        cpp.write_text(source())
        exe = tmp / "probe.exe"
        include = ROOT / "mm/2s2h/SaveManager"
        if pathlib.Path(args.cxx).name.lower() in {"cl", "cl.exe"}:
            command = [args.cxx, "/nologo", "/std:c++20", "/EHsc", "/W4", str(cpp),
                       "/I" + str(include), "/I" + args.json_include, "/Fe:" + str(exe)]
        else:
            command = [args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                       str(cpp), "-I" + str(include), "-I" + args.json_include, "-o", str(exe)]
        subprocess.run(command, check=True, cwd=tmp)
        subprocess.run([str(exe), str(tmp / "saves")], check=True, cwd=tmp)


if __name__ == "__main__":
    main()
