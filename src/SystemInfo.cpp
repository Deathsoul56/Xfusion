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
        {System::SG1000, "SG-1000", {"sg", "sc"}, InputGroup::MasterSystem},
        {System::MasterSystem, "Master System", {"sms"}, InputGroup::MasterSystem},
        {System::GameGear, "Game Gear", {"gg"}, InputGroup::MasterSystem},
        {System::Genesis, "Genesis", {"md", "gen", "smd", "bin"}, InputGroup::Genesis},
        {System::SegaCD, "Sega CD", {"cue", "iso", "chd"}, InputGroup::Genesis},
        {System::Sega32X, "32X", {"32x"}, InputGroup::Genesis},
    };
    return systems;
}

const std::vector<InputGroupDef>& allInputGroups()
{
    static const std::vector<InputGroupDef> groups = {
        {InputGroup::Genesis,
         "genesis",
         {{PadButton::Up, "Up"},
          {PadButton::Down, "Down"},
          {PadButton::Left, "Left"},
          {PadButton::Right, "Right"},
          {PadButton::A, "A"},
          {PadButton::B, "B"},
          {PadButton::C, "C"},
          {PadButton::X, "X", true},
          {PadButton::Y, "Y", true},
          {PadButton::Z, "Z", true},
          {PadButton::Start, "Start"},
          {PadButton::Mode, "Mode", true},
          {PadButton::TurboA, "Turbo A"},
          {PadButton::TurboB, "Turbo B"},
          {PadButton::TurboC, "Turbo C"},
          {PadButton::TurboX, "Turbo X", true},
          {PadButton::TurboY, "Turbo Y", true},
          {PadButton::TurboZ, "Turbo Z", true}},
         true},
        // PicoDrive maps the two pad buttons of these systems onto B and C.
        {InputGroup::MasterSystem,
         "master_system",
         {{PadButton::Up, "Up"},
          {PadButton::Down, "Down"},
          {PadButton::Left, "Left"},
          {PadButton::Right, "Right"},
          {PadButton::B, "Button 1"},
          {PadButton::C, "Button 2"},
          {PadButton::Start, "Start / Pause"},
          {PadButton::TurboB, "Turbo Button 1"},
          {PadButton::TurboC, "Turbo Button 2"}}},
    };
    return groups;
}

const InputGroupDef& inputGroupDef(InputGroup id)
{
    for (const InputGroupDef& def : allInputGroups())
        if (def.id == id)
            return def;
    return allInputGroups().front();
}

QString inputGroupTitle(InputGroup id)
{
    QStringList names;
    for (const SystemDef& def : allSystems())
        if (def.inputGroup == id)
            names << def.folder;
    return names.join(" / ");
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
