#pragma once

#include <array>
#include <map>
#include <string>

#include <QString>
#include <QStringList>

#include "PadButton.h"

// Everything that is specific to one libretro core. The rest of the frontend must not
// hardcode core names, option keys or button ids.
struct CoreProfile {
    QString id;
    QString libraryBaseName;
    // Core options forced to a fixed value; any other option falls back to the core's default.
    std::map<std::string, std::string> options;
    // Some cores keep state across games and need a fresh load for each one.
    bool reloadPerGame = false;
    // Sega CD BIOS file names the core looks for in the system directory, by disc region.
    QStringList segaCdBiosFiles;
    // libretro joypad id for each PadButton, indexed by the enum value.
    std::array<unsigned, kPadButtonCount> retroIds{};

    unsigned retroId(PadButton button) const { return retroIds[static_cast<std::size_t>(button)]; }
    QString libraryFileName() const;
};

const CoreProfile& picoDriveProfile();
const CoreProfile& defaultCoreProfile();
