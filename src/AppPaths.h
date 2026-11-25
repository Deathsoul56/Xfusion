#pragma once

#include "AppSettings.h"
#include "SystemInfo.h"

#include <QString>
#include <QStringList>

namespace AppPaths {

QString dataRoot();
// Where the user chose to keep each kind of file (Settings > Files). Set at startup and when the settings change.
void setFiles(const AppSettings& settings);

// `romDir` is the folder of the game being opened, for the "ROM folder" choice. Everything else goes in a folder per
// system (ADR-0016), next to the executable by default.
QString savesDir(System system, const QString& romDir);
// Created when a state is written, not here.
QString statesDir(System system, const QString& romDir);
QString screenshotsDir(System system, const QString& romDir);
// Where to look for the patch of a game, first match wins: the folder of the game, or the patches folder (next to the
// program by default, Settings > Files) with the folder of the system inside it tried first. The patches folder is
// created when it is the default one.
QStringList patchFolders(System system, const QString& romDir);
// The BIOS folder: next to the executable unless a custom folder was chosen.
QString systemDir();
// Where the user keeps shader presets (.slangp) and their shaders; created empty. Nothing is shipped there.
QString shadersDir();
// The compiled shaders, kept so that a preset is compiled once.
QString shaderCacheDir();
QString coresDir();
QString configDir();
QString inputConfigPath();
QString settingsPath();
// Play time, last played and the scan cache of the library.
QString libraryPath();
// The file of a core (CoreRules.h id) in the cores folder, and whether it is there.
QString corePath(const QString& coreId);
bool coreInstalled(const QString& coreId);

}
