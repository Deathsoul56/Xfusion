#include "InputManager.h"

#include <SDL3/SDL.h>
#include <QtGlobal>
#include <algorithm>
#include <cstdio>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

constexpr int kAxisThreshold = 16384;
constexpr uint64_t kRefreshIntervalNs = 500'000'000;

struct GamepadDefault {
    PadButton button;
    SDL_GamepadButton gamepadButton;
};

const std::vector<GamepadDefault>& gamepadDefaults(InputGroup group)
{
    static const std::vector<GamepadDefault> genesis = {
        {PadButton::Up, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {PadButton::Down, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {PadButton::Left, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {PadButton::Right, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {PadButton::A, SDL_GAMEPAD_BUTTON_WEST},
        {PadButton::B, SDL_GAMEPAD_BUTTON_SOUTH},
        {PadButton::C, SDL_GAMEPAD_BUTTON_EAST},
        {PadButton::X, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {PadButton::Y, SDL_GAMEPAD_BUTTON_NORTH},
        {PadButton::Z, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
        {PadButton::Start, SDL_GAMEPAD_BUTTON_START},
        {PadButton::Mode, SDL_GAMEPAD_BUTTON_BACK},
    };
    static const std::vector<GamepadDefault> masterSystem = {
        {PadButton::Up, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {PadButton::Down, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {PadButton::Left, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {PadButton::Right, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {PadButton::B, SDL_GAMEPAD_BUTTON_SOUTH},
        {PadButton::C, SDL_GAMEPAD_BUTTON_EAST},
        {PadButton::Start, SDL_GAMEPAD_BUTTON_START},
    };
    return group == InputGroup::Genesis ? genesis : masterSystem;
}

std::optional<InputBinding> bindingForGamepadButton(SDL_GamepadBinding* const* bindings, int count, SDL_GamepadButton button)
{
    for (int i = 0; i < count; ++i) {
        const SDL_GamepadBinding& b = *bindings[i];
        if (b.output_type != SDL_GAMEPAD_BINDTYPE_BUTTON || b.output.button != button)
            continue;
        if (b.input_type == SDL_GAMEPAD_BINDTYPE_BUTTON)
            return InputBinding::joyButton(b.input.button);
        if (b.input_type == SDL_GAMEPAD_BINDTYPE_HAT)
            return InputBinding::joyHat(b.input.hat.hat, b.input.hat.hat_mask);
    }
    return std::nullopt;
}

std::optional<int> axisForGamepadAxis(SDL_GamepadBinding* const* bindings, int count, SDL_GamepadAxis axis)
{
    for (int i = 0; i < count; ++i) {
        const SDL_GamepadBinding& b = *bindings[i];
        if (b.output_type == SDL_GAMEPAD_BINDTYPE_AXIS && b.output.axis.axis == axis && b.input_type == SDL_GAMEPAD_BINDTYPE_AXIS)
            return b.input.axis.axis;
    }
    return std::nullopt;
}

}

InputManager::InputManager()
    : m_config(InputConfig::defaults())
{
    // Without this SDL stops updating joysticks whenever its own (nonexistent) window is not focused.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    m_sdlReady = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!m_sdlReady)
        std::fprintf(stderr, "SDL joystick init failed: %s\n", SDL_GetError());
}

InputManager::~InputManager()
{
    for (const Device& device : m_devices)
        SDL_CloseJoystick(device.handle);
    if (m_sdlReady)
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void InputManager::setConfig(const InputConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

InputConfig InputManager::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void InputManager::setKeyDown(int qtKey, bool down)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (down)
        m_keysDown.insert(qtKey);
    else
        m_keysDown.erase(qtKey);
}

void InputManager::clearKeys()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_keysDown.clear();
}

void InputManager::refreshDevices()
{
    if (!m_sdlReady)
        return;
    SDL_UpdateJoysticks();
    const uint64_t now = SDL_GetTicksNS();
    if (!m_refreshDue && now - m_lastRefreshNs < kRefreshIntervalNs)
        return;
    m_refreshDue = false;
    m_lastRefreshNs = now;

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::vector<Device> current;
    for (int i = 0; ids && i < count; ++i) {
        const auto known = std::find_if(m_devices.begin(), m_devices.end(), [&](const Device& d) { return d.id == ids[i]; });
        if (known != m_devices.end()) {
            current.push_back(*known);
            continue;
        }
        if (SDL_Joystick* handle = SDL_OpenJoystick(ids[i])) {
            const char* name = SDL_GetJoystickName(handle);
            current.push_back({ids[i], handle, name ? QString::fromUtf8(name) : QString("Controller")});
        }
    }
    for (const Device& device : m_devices) {
        const bool kept = std::any_of(current.begin(), current.end(), [&](const Device& d) { return d.id == device.id; });
        if (!kept)
            SDL_CloseJoystick(device.handle);
    }
    m_devices = std::move(current);
    SDL_free(ids);
}

// Keys are read from the keyboard, which every player shares; the other inputs belong to the controller the
// player was given, or to nothing when there is none.
bool InputManager::isPressed(const InputBinding& binding, const Device* device, bool keys, bool pads) const
{
    if (binding.type == InputBinding::Type::Key) {
        if (!keys)
            return false;
#ifdef Q_OS_WIN
        if (binding.value != 0)
            return (GetAsyncKeyState(binding.value) & 0x8000) != 0;
#endif
        return m_keysDown.count(binding.code) != 0;
    }
    if (!device || !pads)
        return false;
    switch (binding.type) {
    case InputBinding::Type::JoyButton:
        return SDL_GetJoystickButton(device->handle, binding.code);
    case InputBinding::Type::JoyHat:
        return (SDL_GetJoystickHat(device->handle, binding.code) & binding.value) != 0;
    case InputBinding::Type::JoyAxis: {
        const int value = SDL_GetJoystickAxis(device->handle, binding.code);
        return binding.value < 0 ? value < -kAxisThreshold : value > kAxisThreshold;
    }
    case InputBinding::Type::Key:
    case InputBinding::Type::None:
        break;
    }
    return false;
}

InputManager::Assigned InputManager::assignDevices(const InputConfig& config) const
{
    Assigned assigned{};
    std::vector<bool> taken(m_devices.size(), false);
    // Players that named a controller get the first free one with that name, so two identical pads are told
    // apart by the order they were plugged in.
    for (int player = 0; player < InputConfig::kPlayers; ++player) {
        if (config.device(player).isEmpty())
            continue;
        for (std::size_t i = 0; i < m_devices.size(); ++i) {
            if (!taken[i] && m_devices[i].name == config.device(player)) {
                taken[i] = true;
                assigned[static_cast<std::size_t>(player)] = &m_devices[i];
                break;
            }
        }
    }
    for (int player = 0; player < InputConfig::kPlayers; ++player) {
        if (!config.device(player).isEmpty())
            continue;
        for (std::size_t i = 0; i < m_devices.size(); ++i) {
            if (!taken[i]) {
                taken[i] = true;
                assigned[static_cast<std::size_t>(player)] = &m_devices[i];
                break;
            }
        }
    }
    return assigned;
}

InputManager::PadState InputManager::buttonsFor(InputGroup group, int player, const InputConfig& config, const Device* device, bool keys, bool pads) const
{
    PadState state{};
    for (const InputAction& action : inputGroupDef(group).actions) {
        for (const InputBinding& binding : config.bindings(group, player, action.button)) {
            if (isPressed(binding, device, keys, pads)) {
                state[static_cast<std::size_t>(action.button)] = true;
                break;
            }
        }
    }
    return state;
}

InputManager::PadStates InputManager::poll(InputGroup group, bool windowActive, bool gamepadsInBackground)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    PadStates states{};
    const bool pads = windowActive || gamepadsInBackground;
    if (!pads)
        return states;
    const Assigned devices = assignDevices(m_config);
    for (int player = 0; player < InputConfig::kPlayers; ++player)
        states[static_cast<std::size_t>(player)] = buttonsFor(group, player, m_config, devices[static_cast<std::size_t>(player)], windowActive, true);
    if (m_swapped)
        std::swap(states[0], states[1]);
    return states;
}

void InputManager::setSwapped(bool swapped)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_swapped = swapped;
}

bool InputManager::swapped() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_swapped;
}

bool InputManager::joystickDown(const InputBinding& binding)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    for (const Device& device : m_devices)
        if (isPressed(binding, &device, false, true))
            return true;
    return false;
}

QString InputManager::debugRawState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    QStringList parts;
    for (const Device& device : m_devices) {
        QStringList buttons;
        for (int i = 0, n = SDL_GetNumJoystickButtons(device.handle); i < n; ++i)
            if (SDL_GetJoystickButton(device.handle, i))
                buttons << QString::number(i);
        QString text = QString("dev%1:").arg(device.id);
        if (!buttons.isEmpty())
            text += QString(" btn[%1]").arg(buttons.join(","));
        for (int i = 0, n = SDL_GetNumJoystickHats(device.handle); i < n; ++i) {
            const Uint8 hat = SDL_GetJoystickHat(device.handle, i);
            const char* name = hat == SDL_HAT_UP ? "UP" : hat == SDL_HAT_DOWN ? "DOWN" : hat == SDL_HAT_LEFT ? "LEFT" : hat == SDL_HAT_RIGHT ? "RIGHT" : "?";
            text += QString(" hat[%1=%2]").arg(i).arg(name);
        }
        parts << text;
    }
    return parts.join("  |  ");
}

InputManager::PadState InputManager::pressedWith(InputGroup group, int player, const InputConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    return buttonsFor(group, player, config, assignDevices(config)[static_cast<std::size_t>(player)], true, true);
}

QStringList InputManager::controllerNames()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refreshDue = true;
    refreshDevices();
    QStringList names;
    for (const Device& device : m_devices)
        names << device.name;
    return names;
}

std::vector<InputBinding> InputManager::pressedJoystickInputs() const
{
    std::vector<InputBinding> pressed;
    for (const Device& device : m_devices) {
        for (int i = 0, n = SDL_GetNumJoystickButtons(device.handle); i < n; ++i)
            if (SDL_GetJoystickButton(device.handle, i))
                pressed.push_back(InputBinding::joyButton(i));
        for (int i = 0, n = SDL_GetNumJoystickHats(device.handle); i < n; ++i) {
            const Uint8 hat = SDL_GetJoystickHat(device.handle, i);
            for (int direction : {InputBinding::kHatUp, InputBinding::kHatRight, InputBinding::kHatDown, InputBinding::kHatLeft})
                if (hat & direction)
                    pressed.push_back(InputBinding::joyHat(i, direction));
        }
        for (int i = 0, n = SDL_GetNumJoystickAxes(device.handle); i < n; ++i) {
            const int value = SDL_GetJoystickAxis(device.handle, i);
            if (value > kAxisThreshold)
                pressed.push_back(InputBinding::joyAxis(i, 1));
            else if (value < -kAxisThreshold)
                pressed.push_back(InputBinding::joyAxis(i, -1));
        }
    }
    return pressed;
}

void InputManager::beginCapture()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    m_captureBaseline = pressedJoystickInputs();
}

std::optional<InputBinding> InputManager::captureJoystick()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDevices();
    const std::vector<InputBinding> pressed = pressedJoystickInputs();
    // Inputs released since the capture began may be pressed again.
    std::erase_if(m_captureBaseline, [&](const InputBinding& held) {
        return std::none_of(pressed.begin(), pressed.end(), [&](const InputBinding& now) { return now == held; });
    });
    for (const InputBinding& binding : pressed) {
        if (std::find(m_captureBaseline.begin(), m_captureBaseline.end(), binding) == m_captureBaseline.end())
            return binding;
    }
    return std::nullopt;
}

int InputManager::autoAssign(InputGroup group, int player, int slot, const QString& deviceName, InputConfig& config, QString* message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refreshDue = true;
    refreshDevices();

    const Device* device = nullptr;
    if (deviceName.isEmpty()) {
        device = assignDevices(config)[static_cast<std::size_t>(player)];
    } else {
        for (const Device& candidate : m_devices) {
            if (candidate.name == deviceName) {
                device = &candidate;
                break;
            }
        }
    }
    if (!device) {
        if (message)
            *message = deviceName.isEmpty() ? QString("No controller found for player %1").arg(player + 1) : QString("%1 is not connected").arg(deviceName);
        return 0;
    }
    SDL_Gamepad* gamepad = SDL_IsGamepad(device->id) ? SDL_OpenGamepad(device->id) : nullptr;
    if (!gamepad) {
        if (message)
            *message = "The controller has no standard gamepad layout; bind it by hand";
        return 0;
    }

    const int stickSlot = (slot + 2) % InputConfig::kSlots;
    int count = 0;
    SDL_GamepadBinding** bindings = SDL_GetGamepadBindings(gamepad, &count);
    int assigned = 0;
    for (const GamepadDefault& entry : gamepadDefaults(group)) {
        if (const auto binding = bindingForGamepadButton(bindings, count, entry.gamepadButton)) {
            config.assign(group, player, entry.button, slot, *binding);
            ++assigned;
        }
    }
    const auto leftX = axisForGamepadAxis(bindings, count, SDL_GAMEPAD_AXIS_LEFTX);
    const auto leftY = axisForGamepadAxis(bindings, count, SDL_GAMEPAD_AXIS_LEFTY);
    if (leftX) {
        config.assign(group, player, PadButton::Left, stickSlot, InputBinding::joyAxis(*leftX, -1));
        config.assign(group, player, PadButton::Right, stickSlot, InputBinding::joyAxis(*leftX, 1));
    }
    if (leftY) {
        config.assign(group, player, PadButton::Up, stickSlot, InputBinding::joyAxis(*leftY, -1));
        config.assign(group, player, PadButton::Down, stickSlot, InputBinding::joyAxis(*leftY, 1));
    }
    const char* name = SDL_GetGamepadName(gamepad);
    if (message)
        *message = QString("Assigned %1 controls from %2").arg(assigned).arg(name ? name : "the gamepad");
    SDL_free(static_cast<void*>(bindings));
    SDL_CloseGamepad(gamepad);
    return assigned;
}
