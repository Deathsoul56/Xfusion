#include "GameHeader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include "ZipRead.h"

namespace {

QString text(const QByteArray& block, int offset, int length)
{
    return QString::fromLatin1(block.mid(offset, length)).simplified();
}

QString productCode(const QString& raw)
{
    // "GM MK-4407 -00": the first two letters say what kind of software it is.
    return raw.startsWith("GM ") || raw.startsWith("MK ") || raw.startsWith("AI ") ? raw.mid(3).simplified() : raw;
}

}

QString GameHeader::regionText() const
{
    if (regions == 0)
        return {};
    const unsigned worldwide = Japan | Americas | Europe;
    if ((regions & worldwide) == worldwide)
        return "World";
    QStringList names;
    if (regions & Japan)
        names << "Japan";
    if (regions & Americas)
        names << "USA";
    if (regions & Europe)
        names << "Europe";
    if (names.isEmpty() && (regions & Asia))
        names << "Asia";
    return names.join(" / ");
}

QString GameHeader::company() const
{
    QString text = copyright.simplified();
    if (text.startsWith("(C)", Qt::CaseInsensitive))
        text = text.mid(3).trimmed();
    const QString code = text.section(' ', 0, 0);
    if (code.isEmpty() || !code[0].isLetterOrNumber())
        return {};
    return code.compare("SEGA", Qt::CaseInsensitive) == 0 ? QString("Sega") : code;
}

unsigned parseRegionField(const QByteArray& field)
{
    unsigned regions = 0;
    const QByteArray trimmed = field.left(16).trimmed();
    if (trimmed.startsWith("SEGA")) // some headers repeat the system name here instead of a region
        return 0;
    // The letters win: "E" is Europe here, not the hexadecimal digit 14.
    bool letters = false;
    for (char c : trimmed) {
        switch (c) {
        case 'J': regions |= GameHeader::Japan; letters = true; break;
        case 'U': regions |= GameHeader::Americas; letters = true; break;
        case 'E': regions |= GameHeader::Europe; letters = true; break;
        default: break;
        }
    }
    if (letters || trimmed.isEmpty())
        return regions;
    const char digit = trimmed.at(0);
    int value = -1;
    if (digit >= '0' && digit <= '9')
        value = digit - '0';
    else if (digit >= 'A' && digit <= 'F')
        value = digit - 'A' + 10;
    else if (digit >= 'a' && digit <= 'f')
        value = digit - 'a' + 10;
    return value < 0 ? 0u : unsigned(value) & 0xF;
}

GameHeader parseMegaDriveHeader(const QByteArray& block)
{
    GameHeader header;
    if (block.size() < 0x100 || !block.startsWith("SEGA"))
        return header;
    header.valid = true;
    header.systemType = text(block, 0x00, 0x10);
    header.copyright = text(block, 0x10, 0x10);
    header.domesticTitle = text(block, 0x20, 0x30);
    header.overseasTitle = text(block, 0x50, 0x30);
    header.product = productCode(text(block, 0x80, 0x0E));
    header.regions = parseRegionField(block.mid(0xF0, 16));
    return header;
}

GameHeader parseCartridgeHeader(const QByteArray& head, const QString& extension)
{
    const QString ext = extension.toLower();
    if (ext == "sms" || ext == "gg" || ext == "sg" || ext == "sc") {
        for (int end : {0x8000, 0x4000, 0x2000}) {
            if (head.size() < end)
                continue;
            const QByteArray tail = head.mid(end - 0x10, 0x10);
            if (!tail.startsWith("TMR SEGA"))
                continue;
            GameHeader header;
            header.valid = true;
            const int machine = (static_cast<unsigned char>(tail[0xF]) >> 4);
            header.systemType = machine >= 5 ? "Game Gear" : "Master System";
            switch (machine) {
            case 3: case 5: header.regions = GameHeader::Japan; break;
            case 4: case 6: header.regions = GameHeader::Americas | GameHeader::Europe; break;
            case 7: header.regions = GameHeader::Japan | GameHeader::Americas | GameHeader::Europe; break;
            default: break;
            }
            const auto bcd = [&](int index) { return QString("%1%2").arg(static_cast<unsigned char>(tail[index]) >> 4).arg(static_cast<unsigned char>(tail[index]) & 0xF); };
            header.product = QString("%1%2%3").arg(static_cast<unsigned char>(tail[0xE]) >> 4).arg(bcd(0xD)).arg(bcd(0xC)).remove(QRegularExpression("^0+(?=.)"));
            return header;
        }
        return {};
    }

    // A copier may put 512 bytes in front of the game.
    for (int offset : {0x100, 0x300}) {
        const GameHeader header = parseMegaDriveHeader(head.mid(offset, 0x100));
        if (header.valid)
            return header;
    }
    return {};
}

GameHeader readCartridgeHeader(const QString& path)
{
    constexpr int kHead = 0x8000;
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "zip") {
        static const QStringList gameExtensions = {"md", "gen", "smd", "bin", "32x", "sms", "gg", "sg", "sc"};
        for (const ZipEntry& entry : zipEntries(path)) {
            const QString entryExt = QFileInfo(entry.name).suffix().toLower();
            if (gameExtensions.contains(entryExt))
                return parseCartridgeHeader(zipReadHead(path, entry, kHead), entryExt);
        }
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parseCartridgeHeader(file.read(kHead), ext);
}