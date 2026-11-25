#include "AchievementsConsole.h"

#include "rc_consoles.h"

uint32_t achievementsConsoleId(System system)
{
    switch (system) {
    case System::SG1000: return RC_CONSOLE_SG1000;
    case System::MasterSystem: return RC_CONSOLE_MASTER_SYSTEM;
    case System::GameGear: return RC_CONSOLE_GAME_GEAR;
    case System::Genesis: return RC_CONSOLE_MEGA_DRIVE;
    case System::SegaCD: return RC_CONSOLE_SEGA_CD;
    case System::Sega32X: return RC_CONSOLE_SEGA_32X;
    }
    return RC_CONSOLE_UNKNOWN;
}
