#include "GpuFilters.h"

QString gpuFilterPreset(VideoFilter filter)
{
    const char* name = nullptr;
    switch (filter) {
    case VideoFilter::Simple2x: name = "simple2x"; break;
    case VideoFilter::Simple3x: name = "simple3x"; break;
    case VideoFilter::Simple4x: name = "simple4x"; break;
    case VideoFilter::EPX: name = "epx"; break;
    case VideoFilter::HQ2x: name = "hq2x"; break;
    case VideoFilter::HQ3x: name = "hq3x"; break;
    case VideoFilter::HQ4x: name = "hq4x"; break;
    case VideoFilter::XBRZ2x: name = "xbrz2x"; break;
    case VideoFilter::XBRZ3x: name = "xbrz3x"; break;
    case VideoFilter::XBRZ4x: name = "xbrz4x"; break;
    case VideoFilter::XBRZ5x: name = "xbrz5x"; break;
    case VideoFilter::XBRZ6x: name = "xbrz6x"; break;
    case VideoFilter::SharpBilinear4x: name = "sharpbilinear4x"; break;
    default: break;
    }
    return name ? QString(":/gpu-filters/%1.slangp").arg(name) : QString();
}

QStringList allGpuFilterPresets()
{
    QStringList presets;
    for (int filter = 0; filter <= static_cast<int>(VideoFilter::NTSCRGB); ++filter) {
        const QString preset = gpuFilterPreset(static_cast<VideoFilter>(filter));
        if (!preset.isEmpty())
            presets << preset;
    }
    return presets;
}
