#ifndef COSMETIC_SHADING_H
#define COSMETIC_SHADING_H

#include <stdint.h>

#include "color.h"

enum SHADE_MODE {
    MODE_REVERT,
    MODE_AVG,
    MODE_MIN,
    MODE_MAX,
};

void ShadePaletteNewBase(const char* path, uint32_t begin, uint32_t end, Color_RGBA8 newBase, SHADE_MODE mode);
void ShadePaletteWhite(const char* path, uint32_t begin, uint32_t end, SHADE_MODE mode);
void ShadePaletteRevert(const char* path, uint32_t begin, uint32_t end);
void ShadePaletteGradient(const char* path, uint32_t begin, uint32_t end, Color_RGBA8 oldBase, Color_RGBA8 newBase,
                          Color_RGBA8 targetEnd);
void ShadePaletteTintMatchingHue(const char* path, uint32_t begin, uint32_t end, Color_RGBA8 match,
                                 Color_RGBA8 newBase);
void ShadePaletteTintRevert(const char* path, uint32_t begin, uint32_t end);
void ShadeRGBA16NewBase(const char* path, uint32_t begin, uint32_t end, Color_RGBA8 newBase, SHADE_MODE mode);
void ShadeRGBA16Recolor(const char* path, uint32_t begin, uint32_t end, Color_RGBA8 newBase, f32 minSaturation);
void ShadeRGBA16Revert(const char* path, uint32_t begin, uint32_t end);
uint8_t* ShadeKafeiHairTlut();
void ShadeKafeiHairTlutRevert();

// Copies recover/recolor only caller-owned bytes; native resources and caches are never changed.
bool CosmeticShading_CopyTunicRange(const char* path, uint8_t* bytes, uint32_t size, uint32_t begin, uint32_t end,
                                    Color_RGBA8 base, bool gradient);

#endif // COSMETIC_SHADING_H
