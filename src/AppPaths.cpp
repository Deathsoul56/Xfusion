#include "AppPaths.h"

#include <QDir>
#include <QStandardPaths>

#include "CoreProfile.h"

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

QString configDir()
{
    return ensured(dataRoot() + "/config");
}

QString inputConfigPath()
{
    return configDir() + "/input.json";
}

QString defaultCorePath()
{
    return coresDir() + "/" + defaultCoreProfile().libraryFileName();
}

}
