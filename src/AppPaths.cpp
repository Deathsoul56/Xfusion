#include "AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace AppPaths {

static QString ensured(const QString& path)
{
    QDir().mkpath(path);
    return path;
}

QString dataRoot()
{
    const QString overridePath = qEnvironmentVariable("XFUSION_DATA_DIR");
    if (!overridePath.isEmpty())
        return ensured(QDir(overridePath).absolutePath());
    return ensured(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

QString savesDir(System system)
{
    return ensured(dataRoot() + "/saves/" + systemDef(system).folder);
}

QString systemDir()
{
    return ensured(dataRoot() + "/system");
}

QString coresDir()
{
    return ensured(dataRoot() + "/cores");
}

QString defaultCorePath()
{
    return coresDir() + "/picodrive_libretro.dll";
}

}
