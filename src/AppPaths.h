#pragma once

#include "AppSettings.h"
#include "SystemInfo.h"

#include <QString>

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
// The BIOS folder: next to the executable unless a custom folder was chosen.
QString systemDir();
QString coresDir();
QString configDir();
QString inputConfigPath();
QString settingsPath();
// Play time, last played and the scan cache of the library.
QString libraryPath();
QString defaultCorePath();

}
