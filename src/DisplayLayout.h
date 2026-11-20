#pragma once

#include <QRect>
#include <QSize>

#include "AppSettings.h"

// How the picture is placed, taken from the display settings.
struct DisplayOptions {
    AspectMode aspect = AspectMode::Core;
    bool maintainAspect = true;
    bool stretch = true;
    bool integerScale = false;
};

// Where the picture goes inside the window area. `imageSize` is the size of the frame the core produced and
// `coreAspect` the display aspect ratio it reports.
//  - Without `stretch` the picture keeps its own height, with the aspect applied to the width, centred.
//  - Without `maintainAspect` it fills the whole area.
//  - With integerScale the picture is scaled by a whole number of times the frame height when that fits, and
//    fitted normally when the window is smaller than that.
QRect displayRect(const QSize& area, const QSize& imageSize, double coreAspect, const DisplayOptions& options);
