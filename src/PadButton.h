#pragma once

#include <cstddef>

// SEGA pad buttons as the frontend sees them, independent of any core's libretro mapping.
// The Turbo entries are not buttons of the console: each one holds the bindings that auto-fire the button it names.
enum class PadButton { Up, Down, Left, Right, A, B, C, X, Y, Z, Start, Mode, TurboA, TurboB, TurboC, TurboX, TurboY, TurboZ };

constexpr std::size_t kPadButtonCount = 18;
constexpr std::size_t kConsoleButtonCount = 12; // the buttons before the Turbo entries

constexpr bool isTurbo(PadButton button) { return static_cast<std::size_t>(button) >= kConsoleButtonCount; }
// The button a Turbo entry auto-fires (TurboA is A, TurboX is X, ...).
constexpr PadButton turboTarget(PadButton turbo) { return static_cast<PadButton>(static_cast<int>(turbo) - 8); }

// The Genesis-family pad exists in two versions; X, Y, Z and Mode only exist on the 6-button one.
enum class PadType { ThreeButton, SixButton };
