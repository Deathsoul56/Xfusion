#include "SystemInfo.h"

#include <QFile>
#include <QFileInfo>

namespace {

std::optional<System> systemForExtension(const QString& ext)
{
    for (const SystemDef& def : allSystems())
        if (def.extensions.contains(ext))
            return def.id;
    return std::nullopt;
}

QStringList zipEntryNames(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    constexpr qint64 eocdSize = 22;
    const qint64 tailSize = qMin<qint64>(file.size(), eocdSize + 0xFFFF);
    file.seek(file.size() - tailSize);
    const QByteArray tail = file.read(tailSize);
    const int eocd = tail.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0 || tail.size() - eocd < eocdSize)
        return {};

    auto u16 = [](const QByteArray& b, int at) { return quint16(quint8(b[at]) | quint8(b[at + 1]) << 8); };
    auto u32 = [&](const QByteArray& b, int at) { return quint32(u16(b, at)) | quint32(u16(b, at + 2)) << 16; };

    const int count = u16(tail, eocd + 10);
    file.seek(u32(tail, eocd + 16));
    const QByteArray dir = file.read(u32(tail, eocd + 12));

    QStringList names;
    int pos = 0;
    for (int i = 0; i < count && pos + 46 <= dir.size(); ++i) {
        const int nameLen = u16(dir, pos + 28);
        const int extraLen = u16(dir, pos + 30);
        const int commentLen = u16(dir, pos + 32);
        names << QString::fromUtf8(dir.mid(pos + 46, nameLen));
        pos += 46 + nameLen + extraLen + commentLen;
    }
    return names;
}

}

const std::vector<SystemDef>& allSystems()
{
    static const std::vector<SystemDef> systems = {
        {System::SG1000, "SG-1000", {"sg", "sc"}},
        {System::MasterSystem, "Master System", {"sms"}},
        {System::GameGear, "Game Gear", {"gg"}},
        {System::Genesis, "Genesis", {"md", "gen", "smd", "bin"}},
        {System::SegaCD, "Sega CD", {"cue", "iso", "chd"}},
        {System::Sega32X, "32X", {"32x"}},
    };
    return systems;
}

const SystemDef& systemDef(System id)
{
    for (const SystemDef& def : allSystems())
        if (def.id == id)
            return def;
    return allSystems().front();
}

std::optional<System> systemForFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext != "zip")
        return systemForExtension(ext);

    for (const QString& name : zipEntryNames(path))
        if (const auto system = systemForExtension(QFileInfo(name).suffix().toLower()))
            return system;
    return std::nullopt;
}

QString romFileDialogFilter()
{
    QStringList patterns;
    for (const SystemDef& def : allSystems())
        for (const QString& ext : def.extensions)
            patterns << "*." + ext;
    patterns << "*.zip";
    return QString("SEGA games (%1);;All files (*)").arg(patterns.join(' '));
}
