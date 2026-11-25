#pragma once

#include <QRect>
#include <QSize>

#include "AppSettings.h"

// How the picture is placed, taken from the display settings.
struct DisplayOptions {
    AspectMode aspect = AspectMode::Core;
    bool maintainAspect = true;
    bool stretch = false;
    bool integerScale = false;
};

// Where the picture goes inside the window area. `imageSize` is the size of the frame the core produced and
// `coreAspect` the display aspect ratio it reports.
//  - By default the picture is fitted inside the area, as large as it goes, keeping the aspect ratio, and centred.
//  - With `stretch`, or without `maintainAspect`, it fills the whole area whatever its proportions.
//  - With integerScale the picture is scaled by a whole number of times the frame height when that fits, and
//    fitted normally when the window is smaller than that.
QRect displayRect(const QSize& area, const QSize& imageSize, double coreAspect, const DisplayOptions& options);
