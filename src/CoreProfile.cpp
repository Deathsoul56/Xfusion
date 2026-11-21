#include "CoreProfile.h"

#include "CoreRules.h"
#include "libretro.h"

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
