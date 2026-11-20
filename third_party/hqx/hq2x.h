#pragma once

#include <cstdint>

// hqx filters (Maxim Stepin), as ported to FFmpeg by Clement Bosch (ISC license, see hq2x.cpp). Pixels are RGB565.
bool S9xBlitHQ2xFilterInit(void);
void S9xBlitHQ2xFilterDeinit(void);
void HQ2X_16(uint8_t* in, int inPitch, uint8_t* out, int outPitch, int width, int height);
void HQ3X_16(uint8_t* in, int inPitch, uint8_t* out, int outPitch, int width, int height);
void HQ4X_16(uint8_t* in, int inPitch, uint8_t* out, int outPitch, int width, int height);
