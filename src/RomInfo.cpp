#include "RomInfo.h"

#include <QRegularExpression>
#include <zlib.h>

Checksum cartridgeChecksum(const QByteArray& rom)
{
    if (rom.size() < 0x200 || !rom.mid(0x100, 4).startsWith("SEGA"))
        return Checksum::None;
    const auto byte = [&](qsizetype at) { return static_cast<unsigned char>(rom[at]); };
    const unsigned stored = unsigned(byte(0x18E) << 8 | byte(0x18F));
    // The header says where the ROM ends; a bad value means the whole file.
    qsizetype end = rom.size();
    const quint32 declared = quint32(byte(0x1A4)) << 24 | quint32(byte(0x1A5)) << 16 | quint32(byte(0x1A6)) << 8 | byte(0x1A7);
    if (declared >= 0x1FF && qsizetype(declared) + 1 <= rom.size())
        end = qsizetype(declared) + 1;
    unsigned sum = 0;
    for (qsizetype i = 0x200; i + 1 < end; i += 2)
        sum += byte(i) << 8 | byte(i + 1);
    if (end % 2 != 0 && end > 0x200)
        sum += byte(end - 1) << 8;
    return (sum & 0xFFFF) == stored ? Checksum::Ok : Checksum::Invalid;
}

quint32 crc32Of(const QByteArray& data)
{
    return quint32(crc32(0, reinterpret_cast<const Bytef*>(data.constData()), uInt(data.size())));
}

QString sizeInBits(qint64 bytes)
{
    const qint64 bits = bytes * 8;
    if (bits >= 1024 * 1024)
        return QString("%1Mbits").arg(bits / (1024 * 1024));
    return QString("%1Kbits").arg(bits / 1024);
}

QString revisionOfProduct(const QString& product)
{
    static const QRegularExpression revision("-(\\d{1,2})\\s*$");
    const auto match = revision.match(product);
    return match.hasMatch() ? QString("1.%1").arg(match.captured(1).toInt()) : QString();
}

QString startMessage(const StartInfo& info)
{
    QString text = QString("\"%1\" (%2)").arg(info.title, info.pal ? "PAL" : "NTSC");
    if (!info.revision.isEmpty())
        text += " version " + info.revision;
    text += QString("\n%1: %2: %3, SRAM: %4\n").arg(info.system, info.core, info.size, info.sram);

    QStringList parts;
    if (!info.product.isEmpty())
        parts << "ID: " + info.product;
    if (info.hasCrc)
        parts << QString("CRC32: %1").arg(info.crc, 8, 16, QLatin1Char('0'));
    if (info.checksum == Checksum::Ok)
        parts << "Checksum OK";
    else if (!info.patch.isEmpty())
        parts << info.patch + " patched";
    else if (info.checksum == Checksum::Invalid)
        parts << "Invalid Checksum";
    return text + parts.join(", ");
}
