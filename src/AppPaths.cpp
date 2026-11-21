#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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

namespace {

AppSettings g_files; // only the folder choices are read

QString resolve(const FolderSetting& folder, const QString& subfolder, System system, const QString& romDir)
{
    if (folder.location == FileLocation::RomFolder && !romDir.isEmpty())
        return romDir;
    if (folder.location == FileLocation::Custom && !folder.custom.isEmpty())
        return folder.custom + "/" + systemDef(system).folder;
    return dataRoot() + "/" + subfolder + "/" + systemDef(system).folder;
}

}

void setFiles(const AppSettings& settings)
{
    g_files = settings;
}

QString savesDir(System system, const QString& romDir)
{
    return ensured(resolve(g_files.sramFolder, "saves", system, romDir));
}

QString statesDir(System system, const QString& romDir)
{
    return resolve(g_files.stateFolder, "states", system, romDir);
}

QString screenshotsDir(System system, const QString& romDir)
{
    return resolve(g_files.screenshotFolder, "screenshots", system, romDir);
}

QString systemDir()
{
    if (g_files.biosFolder.location == FileLocation::Custom && !g_files.biosFolder.custom.isEmpty())
        return ensured(g_files.biosFolder.custom);
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

QString settingsPath()
{
    return configDir() + "/settings.json";
}

QString libraryPath()
{
    return configDir() + "/library.json";
}

QString corePath(const QString& coreId)
{
    return coresDir() + "/" + coreProfileById(coreId).libraryFileName();
}

bool coreInstalled(const QString& coreId)
{
    return QFileInfo::exists(corePath(coreId));
}

QString defaultCorePath()
{
    return coresDir() + "/" + defaultCoreProfile().libraryFileName();
}

}
