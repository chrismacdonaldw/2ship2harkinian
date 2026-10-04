#ifndef ANCHOR_PROFILE_H
#define ANCHOR_PROFILE_H

#include <string>
extern "C" {
#include "z64save.h"
}

namespace AnchorProgress {
// Compatibility fingerprint, not authentication. Progress and creation time do not participate.
std::string ProfileSeed(const ShipSaveInfo& save);
} // namespace AnchorProgress

#endif
