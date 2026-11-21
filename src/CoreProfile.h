#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include <QString>
#include <QStringList>

#include "PadButton.h"

// Everything that is specific to one libretro core. The rest of the frontend must not
// hardcode core names, option keys or button ids.
struct CoreProfile {
    QString id; // the same as CoreInfo::id (CoreRules.h)
    QString libraryBaseName;
    // Core options forced to a fixed value; any other option falls back to the core's default.
    std::map<std::string, std::string> options;
    // Options that select the Genesis-family pad and the value each PadType takes. Empty when the core has none.
    std::vector<std::string> padTypeKeys;
    std::string padTypeValues[2];
    // The value that leaves a port empty (used for the second port when only one controller is plugged in).
    std::string noPadValue;
    const std::string& padTypeValue(PadType type) const { return padTypeValues[static_cast<std::size_t>(type)]; }
    // The libretro device a Genesis-family port is given for each pad type, for cores that choose the pad through
    // retro_set_controller_port_device instead of an option. Zero when the core has none (it is then driven by `padTypeKeys`).
    unsigned padDevices[2] = {0, 0};
    // The core takes a path to the game itself and does not open zip files (libretro asks the frontend to extract
    // them): a zipped game is unpacked to a temporary file first. PicoDrive reads zips on its own.
    bool needsExtractedGames = false;
    // Battery RAM whose reported size is only what the game has written so far (Genesis Plus GX), not a fixed size:
    // a save shorter than the core's maximum is the normal case, not a file from another emulator.
    bool saveRamSizeVaries = false;
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
const CoreProfile& genesisPlusGxProfile();
// The profile of a core by its CoreInfo::id; PicoDrive for an unknown one.
const CoreProfile& coreProfileById(const QString& id);
const CoreProfile& defaultCoreProfile();
