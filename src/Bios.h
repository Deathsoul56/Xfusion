#pragma once

#include <QString>
#include <QStringList>
#include <optional>

// The Sega CD needs its BIOS: one for each region of disc. The core looks for fixed file names in the system folder
// (bios_CD_J.bin, bios_CD_E.bin, bios_CD_U.bin), but dumps are called all sorts of things, so a BIOS is recognised by
// what is inside the file: 128 KB, a Mega-CD boot header, and the region letter that header carries.
enum class BiosRegion { Japan, Europe, USA };

struct BiosFile {
    QString path;
    BiosRegion region;
    QString version; // "1.00S", "2.00W", "2.11X"
};

// Not a BIOS (wrong size, no Mega-CD boot header or an unknown region) gives nothing.
std::optional<BiosFile> identifyBios(const QString& path);

QString biosFileName(BiosRegion region); // "bios_CD_J.bin"
QString biosRegionName(BiosRegion region); // "Japan"

struct BiosStatus {
    bool japan = false;
    bool europe = false;
    bool usa = false;
    bool has(BiosRegion region) const;
};

// Which of the three BIOS files in `systemDir` are there and are what their name says.
BiosStatus checkBios(const QString& systemDir);

// Copies the BIOS files found in `sourceFolder` (not its subfolders) into `systemDir` under the names the core wants,
// for the regions that are still missing. Nothing already there is replaced. Returns what was installed, as
// "bios_CD_J.bin (from <file name>)".
QStringList installBios(const QString& sourceFolder, const QString& systemDir, QString* error = nullptr);
