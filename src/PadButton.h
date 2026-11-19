#pragma once

#include <cstddef>

// SEGA pad buttons as the frontend sees them, independent of any core's libretro mapping.
enum class PadButton { Up, Down, Left, Right, A, B, C, X, Y, Z, Start, Mode };

constexpr std::size_t kPadButtonCount = 12;

// The Genesis-family pad exists in two versions; X, Y, Z and Mode only exist on the 6-button one.
enum class PadType { ThreeButton, SixButton };
