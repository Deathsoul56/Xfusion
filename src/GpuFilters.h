#pragma once

#include <QString>
#include <QStringList>

#include "AppSettings.h"

// The filters that have a version for the GPU (ADR-0032): a preset inside the program that does what the filter of the CPU
// does, so that the CPU is left free. The preset is the path of a resource, empty for a filter that only the CPU has
// (NTSC, 2xSaI, Super 2xSaI, Super Eagle).
QString gpuFilterPreset(VideoFilter filter);

// Every preset above, for the tests.
QStringList allGpuFilterPresets();
