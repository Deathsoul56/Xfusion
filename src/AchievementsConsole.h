#pragma once

#include <cstdint>

#include "SystemInfo.h"

// The id RetroAchievements gives each console (rc_consoles.h): 1 Mega Drive, 9 Sega CD, 10 32X, 11 Master System,
// 15 Game Gear, 33 SG-1000.
uint32_t achievementsConsoleId(System system);
