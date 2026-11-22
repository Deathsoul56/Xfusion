#pragma once

#include <QString>

// What a libretro core says about itself, read without starting it: its name and version, and whether it speaks the
// libretro API this frontend does. False, with `error`, when the file is not a usable core.
bool probeCore(const QString& libraryPath, QString* name, QString* version, QString* error);
