#include "SystemInfo.h"

#include "DiscInfo.h"
#include "GameHeader.h"
#include "ZipRead.h"

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

// The extension is only a first guess for a cartridge: a .bin or .md may be a 32X game, and a file called .sms may be
// a Game Gear one. The header, when there is one, has the last word.
System refineByHeader(System guessed, const GameHeader& header)
{
    if (!header.valid)
        return guessed;
    if (guessed == System::Genesis && header.systemType.startsWith("SEGA 32X"))
        return System::Sega32X;
    if (guessed == System::MasterSystem || guessed == System::GameGear)
        return header.systemType == "Game Gear" ? System::GameGear : System::MasterSystem;
    return guessed;
}

QStringList zipEntryNames(const QString& path)
{
    QStringList names;
    for (const ZipEntry& entry : zipEntries(path))
        names << entry.name;
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
    // Discs are told by their first sector, not by the extension: a .bin may be a cartridge or a raw disc track, and
    // an .iso may be a game for another console.
    if (ext == "cue" || ext == "iso" || ext == "chd" || ext == "bin") {
        if (readDisc(path).isSegaCd)
            return System::SegaCD;
        if (ext != "bin")
            return std::nullopt;
        return refineByHeader(System::Genesis, readCartridgeHeader(path));
    }
    if (ext != "zip") {
        const auto system = systemForExtension(ext);
        return system ? std::optional<System>(refineByHeader(*system, readCartridgeHeader(path))) : std::nullopt;
    }

    for (const ZipEntry& entry : zipEntries(path)) {
        const QString entryExt = QFileInfo(entry.name).suffix().toLower();
        if (const auto system = systemForExtension(entryExt))
            return refineByHeader(*system, parseCartridgeHeader(zipReadHead(path, entry, 0x8000), entryExt));
    }
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
