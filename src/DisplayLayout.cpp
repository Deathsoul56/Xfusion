#include "DisplayLayout.h"

#include <algorithm>
#include <cmath>

QRect displayRect(const QSize& area, const QSize& imageSize, double coreAspect, const DisplayOptions& options)
{
    if (area.isEmpty() || imageSize.isEmpty())
        return {};

    double aspect = coreAspect;
    if (options.aspect == AspectMode::FourThree)
        aspect = 4.0 / 3.0;
    else if (options.aspect == AspectMode::Square)
        aspect = double(imageSize.width()) / imageSize.height();

    if (!options.stretch) {
        const int height = imageSize.height();
        const int width = int(std::lround(height * aspect));
        return QRect((area.width() - width) / 2, (area.height() - height) / 2, width, height);
    }
    if (!options.maintainAspect)
        return QRect(QPoint(0, 0), area);

    int width = area.width();
    int height = int(width / aspect);
    if (height > area.height()) {
        height = area.height();
        width = int(height * aspect);
    }

    if (options.integerScale) {
        const int scale = height / imageSize.height();
        if (scale >= 1) {
            height = imageSize.height() * scale;
            width = int(std::lround(height * aspect));
        }
    }
    width = std::min(width, area.width());
    return QRect((area.width() - width) / 2, (area.height() - height) / 2, width, height);
}
