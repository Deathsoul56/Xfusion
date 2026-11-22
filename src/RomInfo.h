#pragma once

#include <QByteArray>
#include <QString>

// The text shown when a game starts (Options > General > Show game info), laid out like the one snes9xrd shows for a
// ROM: three lines with the name, the video standard and the version, then what the game is and how big, then its id,
// CRC32 and whether its checksum is right.
enum class Checksum { Ok, Invalid, None }; // None: the format has no checksum we can check

// The Mega Drive / 32X checksum (the 16 bit words after the header, big endian) against the one in the header.
Checksum cartridgeChecksum(const QByteArray& rom);

quint32 crc32Of(const QByteArray& data);

// "4Mbits", or "256Kbits" under a megabit.
QString sizeInBits(qint64 bytes);

// "1.0" for a product code ending in "-00", "1.1" for "-01"; empty when the code has no revision.
QString revisionOfProduct(const QString& product);

struct StartInfo {
    QString title; // the name in the game's header, or the file name
    bool pal = false;
    QString revision; // revisionOfProduct()
    QString system; // "Genesis"
    QString core;
    QString size; // sizeInBits(), or what a disc is made of
    QString sram; // "8KB" or "none"
    QString product;
    bool hasCrc = false; // a disc is not read through just for this
    quint32 crc = 0;
    Checksum checksum = Checksum::None;
    QString patch; // "IPS", "BPS" or "UPS" when the game was patched
};

QString startMessage(const StartInfo& info);
