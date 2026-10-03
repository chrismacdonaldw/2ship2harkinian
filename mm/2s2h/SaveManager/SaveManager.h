
#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#ifdef __cplusplus
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
extern "C" {
#include "z64save.h"
}

enum class SaveManagerReadStatus {
    Ok, MissingFile, ReadError, MalformedJson, FutureVersion, MigrationError,
    MissingHalf, IncompatibleRandomizer, ConversionError
};
struct SaveManagerProbeResult {
    SaveManagerReadStatus status = SaveManagerReadStatus::ReadError;
    int version = 0;
    bool hasNewCycle = false;
    bool hasOwl = false;
    std::optional<ShipSaveInfo> newCycle;
    std::optional<ShipSaveInfo> owl;
    std::string error;
};
// Reads and validates every present half without modifying files or live game state.
// Evidence owns its native metadata; half selection and paired-file policy belong to the caller.
SaveManagerProbeResult SaveManager_ProbeSaveFile(const std::filesystem::path& path);
std::string SaveManager_GetFileName(int fileNum, bool isBackup = false);
bool SaveManager_HandleFileDropped(char* filePath);
bool BinarySaveConverter_HandleFileDropped(char* filePath);
int SaveManager_GetOpenFileSlot();
bool SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j);
void SaveManager_PersistSariaHintsAvailable();
#else
void SaveManager_SysFlashrom_WriteData(u8* addr, u32 pageNum, u32 pageCount);
s32 SaveManager_SysFlashrom_ReadData(void* addr, u32 pageNum, u32 pageCount);
#endif

#endif // SAVE_MANAGER_H
