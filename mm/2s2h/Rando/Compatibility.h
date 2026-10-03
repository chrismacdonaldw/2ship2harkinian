#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace Rando::Compatibility {
// Bump when persisted randomizer IDs, layouts or meanings change, not for build-only changes.
inline constexpr const char* kField = "randoCompatibility";
inline constexpr const char* kSchema = "diptych-mm-rando-1";

inline bool ValidHash(std::string_view hash) {
    if (hash.size() != 7) return false;
    for (char c : hash) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

inline bool ReadSaveHash(const nlohmann::json& source, std::string& hash) {
    const auto entry = source.find("commitHash");
    if (entry == source.end() || !entry->is_array() || entry->size() != 8) return false;
    std::string bounded;
    for (size_t i = 0; i < 8; ++i) {
        const auto& value = (*entry)[i];
        if (!value.is_number_integer()) return false;
        if (value.is_number_unsigned() ? value.get<uint64_t>() > 255 :
            (value.get<int64_t>() < 0 || value.get<int64_t>() > 255)) return false;
        const auto byte = value.get<unsigned>();
        if (i == 7) {
            if (byte != 0) return false;
        } else {
            bounded.push_back(static_cast<char>(byte));
        }
    }
    if (!ValidHash(bounded)) return false;
    hash = bounded;
    return true;
}

inline bool Accept(const nlohmann::json& source, std::string_view hash, std::string_view current) {
    if (!source.is_object() || !ValidHash(hash) || !ValidHash(current)) return false;
#ifdef DIPTYCH_GAME_MODULE
    const auto schema = source.find(kField);
    // An explicit unsupported schema must never fall through to the current-build check.
    if (schema != source.end()) return schema->is_string() && schema->get<std::string>() == kSchema;
    // Exact reviewed imports preserve the original randomizer layouts and enum meanings.
    return hash == current || hash == "e8757c1" || hash == "ae8531a";
#else
    return hash == current;
#endif
}

inline bool Save(const nlohmann::json& source, std::string_view current, std::string& hash) {
    return source.is_object() && ReadSaveHash(source, hash) && Accept(source, hash, current);
}

inline bool Half(const nlohmann::json& source, std::string_view current, std::string& hash) {
    if (!source.is_object()) return false;
    const auto entry = source.find("commitHash");
    if (entry == source.end() || !entry->is_string()) return false;
    hash = entry->get<std::string>();
    return Accept(source, hash, current);
}
} // namespace Rando::Compatibility
