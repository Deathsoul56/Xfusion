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

    InputManager();
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void setConfig(const InputConfig& config);
    InputConfig config() const;

    // Nothing is pressed while the window is not active, so typing elsewhere never reaches the game.
    PadState poll(InputGroup group, bool windowActive);

    // Keyboard state from Qt events; used for keys without a Windows virtual key and on other platforms.
    void setKeyDown(int qtKey, bool down);
    void clearKeys();

    QStringList controllerNames();

    // Capture: the first joystick input pressed after beginCapture(); inputs already held are ignored.
    void beginCapture();
    std::optional<InputBinding> captureJoystick();

    // Fills the controller slots of `group` from the standard gamepad layout of the first recognized
    // gamepad. Returns the number of actions that received a binding.
    int autoAssign(InputGroup group, InputConfig& config, QString* message);

private:
    struct Device {
        uint32_t id;
        SDL_Joystick* handle;
    };

    void refreshDevices();
    std::vector<InputBinding> pressedJoystickInputs() const;
    bool isPressed(const InputBinding& binding) const;

    mutable std::mutex m_mutex;
    InputConfig m_config;
    std::vector<Device> m_devices;
    std::set<int> m_keysDown;
    std::vector<InputBinding> m_captureBaseline;
    uint64_t m_lastRefreshNs = 0;
    bool m_refreshDue = true;
    bool m_sdlReady = false;
};
