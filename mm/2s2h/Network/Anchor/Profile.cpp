#include "Profile.h"
#include <nlohmann/json.hpp>
#include <ship/utils/StrHash64.h>
#include <iomanip>
#include <sstream>

namespace AnchorProgress {
std::string ProfileSeed(const ShipSaveInfo& save) {
    const auto& rando = save.rando;
    size_t commitLength = 0;
    while (commitLength < sizeof(save.commitHash) && save.commitHash[commitLength] != 0) {
        ++commitLength;
    }
    nlohmann::json profile = { { "schema", "mm-permanent-switches-1" },
                               { "generation", std::string(save.commitHash, commitLength) },
                               { "saveType", save.saveType } };
    if (save.saveType == SAVETYPE_RANDO) {
        profile["finalSeed"] = rando.finalSeed;
        profile["options"] = rando.randoSaveOptions;
        profile["startingItems"] = rando.randoStartingItems;
        profile["priorityItems"] = rando.sariaPriorityItems;
        auto& placements = profile["placements"] = nlohmann::json::array();
        for (size_t id = 1; id < RC_MAX; ++id) {
            const auto& check = rando.randoSaveChecks[id];
            placements.push_back({ id, check.shuffled, check.randoItemId, check.price });
        }
    }
    // json's ordered object keys and numeric arrays avoid struct padding, endianness and mutable save fields.
    const std::string bytes = profile.dump();
    std::ostringstream seed;
    seed << "mm-profile-1:" << std::hex << std::setfill('0') << std::setw(16) << CRC64(bytes.c_str());
    return seed.str();
}
} // namespace AnchorProgress
