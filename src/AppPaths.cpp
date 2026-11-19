#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>

#include "CoreProfile.h"

namespace AppPaths {

static QString ensured(const QString& path)
{
    QDir().mkpath(path);
    return path;
}

QString dataRoot()
{
    // Everything lives next to the executable so the folder can be moved or deleted as a unit.
    const QString overridePath = qEnvironmentVariable("XFUSION_DATA_DIR");
    if (!overridePath.isEmpty())
        return ensured(QDir(overridePath).absolutePath());
    return ensured(QCoreApplication::applicationDirPath());
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
