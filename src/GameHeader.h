#pragma once

#include <QString>
#include <QStringList>

class QByteArray;

// What the 256-byte header at 0x100 of a Mega Drive / 32X cartridge, and of the first sector of a Sega CD disc,
// says about the game. The BIOS files carry the same header.
struct GameHeader {
    // Region bits as the Mega Drive documents them in the hexadecimal form of the field.
    enum Region { Japan = 1, Asia = 2, Americas = 4, Europe = 8 };

    bool valid = false;
    QString systemType; // "SEGA MEGA DRIVE", "SEGA GENESIS", "SEGA 32X", ...
    QString copyright;
    QString domesticTitle;
    QString overseasTitle;
    QString product; // "MK-4407 -00" without the "GM " / "MK " type prefix
    unsigned regions = 0; // 0 when the field says nothing usable

    // The overseas title, or the domestic one when that is empty.
    QString title() const { return overseasTitle.isEmpty() ? domesticTitle : overseasTitle; }
    // "Japan", "USA", "Europe", "Japan / USA" ... or "World" when all three are there.
    QString regionText() const;
    // Who made it, as the copyright field says it ("(C)SEGA 1991.APR" gives "Sega"; another maker gives the code the header
    // has, such as "T-81"); empty when the header has none.
    QString company() const;
};

// `block` is the 256 bytes starting at 0x100. Valid when it starts with "SEGA".
GameHeader parseMegaDriveHeader(const QByteArray& block);

// Region bits from the country field (16 bytes at 0x1F0): the letters J, U and E as the old headers wrote them, or
// a hexadecimal digit, as the newer ones do.
unsigned parseRegionField(const QByteArray& field);

// The header of a cartridge image, from its first 32 KB (or all of it when shorter): Mega Drive and 32X files (with or
// without the 512-byte copier header) and Master System / Game Gear files, whose header is at the end of an 8 KB, 16 KB
// or 32 KB block. `extension` is the one of the image file ("md", "sms", ...).
GameHeader parseCartridgeHeader(const QByteArray& head, const QString& extension);

// The same, read from a game file or from the first game inside a zip. Anything else gives an invalid header.
GameHeader readCartridgeHeader(const QString& path);
