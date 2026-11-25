#include "CoreProfile.h"

#include "CoreRules.h"
#include "libretro.h"

#include <algorithm>

QString CoreProfile::libraryFileName() const
{
#if defined(Q_OS_WIN)
    return libraryBaseName + ".dll";
#elif defined(Q_OS_MACOS)
    return libraryBaseName + ".dylib";
#else
    return libraryBaseName + ".so";
#endif
}

const CoreOption* CoreProfile::userOption(const std::string& key) const
{
    for (const CoreOption& option : userOptions)
        if (option.key == key)
            return &option;
    return nullptr;
}

bool isValidCoreOption(const CoreOption& option, const std::string& value)
{
    return std::any_of(option.values.begin(), option.values.end(), [&](const auto& v) { return v.first == value; });
}

namespace {

CoreOption onOff(std::string key, QString label, QString tip)
{
    return {std::move(key), std::move(label), std::move(tip), {{"disabled", "Off"}, {"enabled", "On"}}, "disabled"};
}

}

const CoreProfile& picoDriveProfile()
{
    static const CoreProfile profile = [] {
        CoreProfile p;
        p.id = "picodrive";
        p.libraryBaseName = "picodrive_libretro";
        p.padTypeKeys = {"picodrive_input1", "picodrive_input2"};
        p.padTypeValues[static_cast<std::size_t>(PadType::ThreeButton)] = "3 button pad";
        p.padTypeValues[static_cast<std::size_t>(PadType::SixButton)] = "6 button pad";
        p.noPadValue = "None";
        p.regionKey = "picodrive_region";
        p.regionValues[0] = "Japan NTSC";
        p.regionValues[1] = "US";
        p.regionValues[2] = "Europe";
        p.userOptions = {
            {"picodrive_region", "System Region", "Which console the game runs on. PAL is 50 Hz and NTSC 60 Hz; the wrong one makes a game run too fast or too slow.",
             {{"Auto", "Auto"}, {"Japan NTSC", "Japan (NTSC)"}, {"Japan PAL", "Japan (PAL)"}, {"US", "US (NTSC)"}, {"Europe", "Europe (PAL)"}}, "Auto"},
            onOff("picodrive_sprlim", "No sprite limit", "Removes the limit of sprites per line. Less flicker, but some games use the limit for effects."),
        };
        // PicoDrive keeps pad latency counters in file-level statics (pico/memory.c) that a new game does not
        // clear, while its cycle counter restarts at zero. A/B read inverted until the new game has run as long as
        // the previous one, so each game after the first gets a freshly loaded core (ADR-0004).
        p.reloadPerGame = true;
        p.segaCdBiosFiles = {"bios_CD_E.bin", "bios_CD_U.bin", "bios_CD_J.bin"};
        // PicoDrive translates libretro ids as B=B, Y=A, A=C, L=X, X=Y, R=Z, Select=Mode.
        auto set = [&p](PadButton button, unsigned id) { p.retroIds[static_cast<std::size_t>(button)] = id; };
        set(PadButton::Up, RETRO_DEVICE_ID_JOYPAD_UP);
        set(PadButton::Down, RETRO_DEVICE_ID_JOYPAD_DOWN);
        set(PadButton::Left, RETRO_DEVICE_ID_JOYPAD_LEFT);
        set(PadButton::Right, RETRO_DEVICE_ID_JOYPAD_RIGHT);
        set(PadButton::A, RETRO_DEVICE_ID_JOYPAD_Y);
        set(PadButton::B, RETRO_DEVICE_ID_JOYPAD_B);
        set(PadButton::C, RETRO_DEVICE_ID_JOYPAD_A);
        set(PadButton::X, RETRO_DEVICE_ID_JOYPAD_L);
        set(PadButton::Y, RETRO_DEVICE_ID_JOYPAD_X);
        set(PadButton::Z, RETRO_DEVICE_ID_JOYPAD_R);
        set(PadButton::Start, RETRO_DEVICE_ID_JOYPAD_START);
        set(PadButton::Mode, RETRO_DEVICE_ID_JOYPAD_SELECT);
        return p;
    }();
    return profile;
}

const CoreProfile& genesisPlusGxProfile()
{
    static const CoreProfile profile = [] {
        CoreProfile p;
        p.id = "genesis_plus_gx";
        p.libraryBaseName = "genesis_plus_gx_libretro";
        // The pad is chosen with retro_set_controller_port_device: RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) is the
        // 3 button pad and (..., 1) the 6 button one.
        p.padDevices[static_cast<std::size_t>(PadType::ThreeButton)] = RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0);
        p.padDevices[static_cast<std::size_t>(PadType::SixButton)] = RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1);
        p.regionKey = "genesis_plus_gx_region_detect";
        p.regionValues[0] = "ntsc-j";
        p.regionValues[1] = "ntsc-u";
        p.regionValues[2] = "pal";
        p.userOptions = {
            {"genesis_plus_gx_region_detect", "System Region", "Which console the game runs on. PAL is 50 Hz and NTSC 60 Hz; the wrong one makes a game run too fast or too slow.",
             {{"auto", "Auto"}, {"ntsc-u", "NTSC-U"}, {"pal", "PAL"}, {"ntsc-j", "NTSC-J"}}, "auto"},
            {"genesis_plus_gx_ym2413", "Master System FM sound", "The FM sound chip of the Mark III and Master System, which some games use for better music.",
             {{"auto", "Auto"}, {"disabled", "Off"}, {"enabled", "On"}}, "auto"},
            onOff("genesis_plus_gx_gg_extra", "Game Gear extended screen", "Runs Game Gear games at 256x192, which can show more of the picture and usually some garbage at the edges."),
            onOff("genesis_plus_gx_lcd_filter", "LCD ghosting", "Blurs fast movement like the LCD of a Game Gear or a Genesis Nomad."),
            {"genesis_plus_gx_overscan", "Borders", "The parts of the picture that a television hid.",
             {{"disabled", "Hidden"}, {"top/bottom", "Top and bottom"}, {"left/right", "Left and right"}, {"full", "All"}}, "disabled"},
            {"genesis_plus_gx_left_border", "Master System side borders", "Cuts 8 pixels off the left, or the left and right, of Master System games.",
             {{"disabled", "Shown"}, {"left border", "Hide the left one"}, {"left & right borders", "Hide both"}}, "disabled"},
            {"genesis_plus_gx_blargg_ntsc_filter", "NTSC filter of the core", "Imitates the signal of a television; the filters of Options > Display work on top of this.",
             {{"disabled", "Off"}, {"monochrome", "Monochrome"}, {"composite", "Composite"}, {"svideo", "S-Video"}, {"rgb", "RGB"}}, "disabled"},
            onOff("genesis_plus_gx_no_sprite_limit", "No sprite limit", "Removes the limit of sprites per line. Less flicker, but some games use the limit for effects."),
        };
        p.saveRamSizeVaries = true;
        p.needsExtractedGames = true;
        p.segaCdBiosFiles = {"bios_CD_E.bin", "bios_CD_U.bin", "bios_CD_J.bin"};
        // Genesis Plus GX reads the pad as: A = Y, B = B, C = A, X = L, Y = X, Z = R, Mode = Select (the same as
        // PicoDrive), so the table is the same.
        auto set = [&p](PadButton button, unsigned id) { p.retroIds[static_cast<std::size_t>(button)] = id; };
        set(PadButton::Up, RETRO_DEVICE_ID_JOYPAD_UP);
        set(PadButton::Down, RETRO_DEVICE_ID_JOYPAD_DOWN);
        set(PadButton::Left, RETRO_DEVICE_ID_JOYPAD_LEFT);
        set(PadButton::Right, RETRO_DEVICE_ID_JOYPAD_RIGHT);
        set(PadButton::A, RETRO_DEVICE_ID_JOYPAD_Y);
        set(PadButton::B, RETRO_DEVICE_ID_JOYPAD_B);
        set(PadButton::C, RETRO_DEVICE_ID_JOYPAD_A);
        set(PadButton::X, RETRO_DEVICE_ID_JOYPAD_L);
        set(PadButton::Y, RETRO_DEVICE_ID_JOYPAD_X);
        set(PadButton::Z, RETRO_DEVICE_ID_JOYPAD_R);
        set(PadButton::Start, RETRO_DEVICE_ID_JOYPAD_START);
        set(PadButton::Mode, RETRO_DEVICE_ID_JOYPAD_SELECT);
        return p;
    }();
    return profile;
}

const CoreProfile& coreProfileById(const QString& id)
{
    return id == CoreId::GenesisPlusGx ? genesisPlusGxProfile() : picoDriveProfile();
}

const CoreProfile& defaultCoreProfile()
{
    return picoDriveProfile();
}
