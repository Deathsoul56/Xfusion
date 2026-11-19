#include "CoreProfile.h"

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
        // The default 3-button pad ignores X/Y/Z.
        p.options = {{"picodrive_input1", "6 button pad"}, {"picodrive_input2", "6 button pad"}};
        // The pad protocol broke when a second game was loaded into the same core instance.
        p.reloadPerGame = true;
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

const CoreProfile& defaultCoreProfile()
{
    return picoDriveProfile();
}
