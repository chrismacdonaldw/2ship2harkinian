#pragma once
#include "ultra64.h"
#include "color.h"
#include "HumanTunic.h"
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;
struct CosmeticFormTunicCache;
enum { COSMETIC_FORM_TUNIC_MATERIALS = 23 };
typedef struct CosmeticFormTunicMaterials {
    const Gfx* originals[COSMETIC_FORM_TUNIC_MATERIALS];
    Gfx* copies[COSMETIC_FORM_TUNIC_MATERIALS];
    const char* paths[COSMETIC_FORM_TUNIC_MATERIALS];
    u8 count;
} CosmeticFormTunicMaterials;
struct CosmeticFormTunicCache* CosmeticEditor_CreateFormTunic(void);
void CosmeticEditor_DestroyFormTunic(struct CosmeticFormTunicCache* cache);
int CosmeticEditor_BuildFormTunic(struct PlayState* play, struct CosmeticFormTunicCache* cache, u8 form,
                                  Color_RGBA8 color, CosmeticFormTunicMaterials* materials);
Gfx* CosmeticEditor_FormTunicDList(const CosmeticFormTunicMaterials* materials, Gfx* original);
void CosmeticEditor_FormTunicPostDraw(const CosmeticFormTunicMaterials* materials, Gfx* begin, Gfx* end);
void CosmeticEditor_TunicPostDraw(const CosmeticHumanTunicMaterials* human,
                                   const CosmeticFormTunicMaterials* form, Gfx* begin, Gfx* end);
#ifdef __cplusplus
}
#endif
