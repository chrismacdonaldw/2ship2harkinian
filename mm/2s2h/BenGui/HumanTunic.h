#pragma once

#include "ultra64.h"
#include "color.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
enum { COSMETIC_HUMAN_TUNIC_MATERIALS = 9 };
typedef struct CosmeticHumanTunicMaterials {
    Gfx* copies[COSMETIC_HUMAN_TUNIC_MATERIALS];
    const Gfx* originals[COSMETIC_HUMAN_TUNIC_MATERIALS];
} CosmeticHumanTunicMaterials;

int CosmeticEditor_GetHumanTunicColor(Color_RGBA8* color);
int CosmeticEditor_BuildHumanTunic(struct PlayState* play, Color_RGBA8 color, CosmeticHumanTunicMaterials* materials);
Gfx* CosmeticEditor_HumanTunicDList(const CosmeticHumanTunicMaterials* materials, Gfx* original);

#ifdef __cplusplus
}
#endif
