#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include "InputBinding.h"
#include "InputConfig.h"

struct SDL_Joystick;

// Reads the keyboard and any connected joystick and turns them into pad buttons using the active bindings.
// All members are thread safe: the emulation thread polls while the GUI thread edits bindings.
class InputManager {
public:
    using PadState = std::array<bool, kPadButtonCount>;
    using PadStates = std::array<PadState, InputConfig::kPlayers>;

    InputManager();
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void setConfig(const InputConfig& config);
    InputConfig config() const;

    // The buttons of every player. While the window is not active the keyboard is ignored, so typing elsewhere
    // never reaches the game; gamepads still count when `gamepadsInBackground` is set.
    PadStates poll(InputGroup group, bool windowActive, bool gamepadsInBackground = false);
    // Swaps what players 1 and 2 control (for this session only; the settings are untouched).
    void setSwapped(bool swapped);
    bool swapped() const;
    // Whether a gamepad binding is held right now on any connected gamepad, for the frontend shortcuts.
    bool joystickDown(const InputBinding& binding);
    // The raw state of every joystick, to see what an unknown pad reports: "dev1:btn[3,4] hat[0=UP]".
    QString debugRawState();
    // The buttons `player` is pressing right now under `config` (the settings dialog's unsaved copy), regardless
    // of which window is active; used to light the buttons of the pad picture.
    PadState pressedWith(InputGroup group, int player, const InputConfig& config);

    // Keyboard state from Qt events; used for keys without a Windows virtual key and on other platforms.
    void setKeyDown(int qtKey, bool down);
    void clearKeys();

    QStringList controllerNames();

    // Capture: the first joystick input pressed after beginCapture(); inputs already held are ignored.
    void beginCapture();
    std::optional<InputBinding> captureJoystick();

    // Fills the controller slots of `group` for `player` from the standard gamepad layout of the controller that
    // player uses under `config`. Returns the number of actions that received a binding.
    // `deviceName` picks the gamepad; empty uses the one the player has. The buttons go to `slot` and the left
    // stick to the slot two places on, so neither overwrites the other.
    int autoAssign(InputGroup group, int player, int slot, const QString& deviceName, InputConfig& config, QString* message);

private:
    struct Device {
        uint32_t id;
        SDL_Joystick* handle;
        QString name;
    };
    using Assigned = std::array<const Device*, InputConfig::kPlayers>;

    void refreshDevices();
    // The controller each player reads, or null when a player has none. The pointers stay valid until the next
    // refreshDevices().
    Assigned assignDevices(const InputConfig& config) const;
    PadState buttonsFor(InputGroup group, int player, const InputConfig& config, const Device* device, bool keys, bool pads) const;
    std::vector<InputBinding> pressedJoystickInputs() const;
    bool isPressed(const InputBinding& binding, const Device* device, bool keys, bool pads) const;

    mutable std::mutex m_mutex;
    InputConfig m_config;
    std::vector<Device> m_devices;
    std::set<int> m_keysDown;
    std::vector<InputBinding> m_captureBaseline;
    uint64_t m_lastRefreshNs = 0;
    bool m_refreshDue = true;
    bool m_sdlReady = false;
    bool m_swapped = false;
};
