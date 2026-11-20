#pragma once

#include <QImage>
#include <vector>

#include "AppSettings.h"

struct VideoFilterInfo {
    VideoFilter id;
    const char* key; // identifier in the settings file
    const char* label; // as snes9xrd names it
};

const std::vector<VideoFilterInfo>& allVideoFilters();
QString videoFilterKey(VideoFilter filter);
VideoFilter videoFilterFromKey(const QString& key, VideoFilter fallback);

// Applies a CPU filter to a frame. The result is what gets drawn; screenshots use the unfiltered frame.
//  - Scanlines: every line twice, the second one darkened.
//  - Simple 2x, 3x, 4x: the frame enlarged with hard edges.
//  - Super Eagle, 2xSaI, Super 2xSaI, EPX: 2x pixel-art scalers that smooth diagonals; written for Xfusion from the
//    published ideas, so they are not bit-exact with the originals (see ADR-0019).
//  - HQ2x, HQ3x, HQ4x (hqx), 2xBRZ to 6xBRZ (xBRZ): third-party scalers in third_party/.
//  - Sharp Bilinear 4x: enlarged by 4 with hard edges, so that the smooth scaling of the window only blends the
//    small remainder and the pixels stay sharp.
//  - NTSC (Composite, RF, S-Video, RGB): Blargg's snes_ntsc, which emulates the analogue video chain of a console
//    (made for the SNES; the signal timing of the Mega Drive differs, so it is an approximation there).
QImage applyVideoFilter(const QImage& frame, VideoFilter filter);
