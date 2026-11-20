#pragma once

#include <QJsonObject>
#include <QString>
#include <array>
#include <vector>

#include "InputBinding.h"

// Actions of the frontend itself (not of the emulated pad) that can be bound to keys and gamepad inputs. The
// quick slot actions are consecutive so that slot N is QuickSave0 + N.
enum class ShortcutAction {
    OpenRom,
    FastForward,
    ToggleFastForward,
    PauseContinue,
    Reset,
    PowerCycle,
    Quit,
    ToggleFullscreen,
    Screenshot,
    SaveState,
    LoadState,
    UndoState,
    IncreaseSlot,
    DecreaseSlot,
    QuickSave0,
    QuickSave1,
    QuickSave2,
    QuickSave3,
    QuickSave4,
    QuickSave5,
    QuickSave6,
    QuickSave7,
    QuickSave8,
    QuickSave9,
    QuickLoad0,
    QuickLoad1,
    QuickLoad2,
    QuickLoad3,
    QuickLoad4,
    QuickLoad5,
    QuickLoad6,
    QuickLoad7,
    QuickLoad8,
    QuickLoad9,
    Rewind,
    SwapControllers,
    SpeedUp,
    SpeedDown,
    FrameSkipUp,
    FrameSkipDown,
    FrameAdvance,
    Mute,
    VolumeDown,
    VolumeUp,
};

constexpr int kShortcutCount = static_cast<int>(ShortcutAction::VolumeUp) + 1;
constexpr int kQuickSlots = 10;

constexpr ShortcutAction quickSaveAction(int slot) { return static_cast<ShortcutAction>(static_cast<int>(ShortcutAction::QuickSave0) + slot); }
constexpr ShortcutAction quickLoadAction(int slot) { return static_cast<ShortcutAction>(static_cast<int>(ShortcutAction::QuickLoad0) + slot); }

struct ShortcutDef {
    ShortcutAction id;
    QString key; // stable identifier used in the settings file
    QString label;
    bool hold; // active while the input is down (fast-forward, rewind); every other action fires on press
};

const std::vector<ShortcutDef>& allShortcuts();
const ShortcutDef& shortcutDef(ShortcutAction id);

// Up to kSlots bindings per action, mixing keys (with exact modifiers) and gamepad inputs. Nothing stops the same
// input from being used twice: the settings page highlights it instead, as snes9xrd does.
class ShortcutMap {
public:
    static constexpr int kSlots = 4;
    using Slots = std::array<InputBinding, kSlots>;

    static ShortcutMap defaults();

    const Slots& bindings(ShortcutAction action) const { return m_actions[static_cast<std::size_t>(action)]; }
    void set(ShortcutAction action, int slot, const InputBinding& binding) { m_actions[static_cast<std::size_t>(action)][slot] = binding; }
    void clearSlot(ShortcutAction action, int slot) { set(action, slot, {}); }
    void clearAction(ShortcutAction action) { m_actions[static_cast<std::size_t>(action)] = {}; }
    // Empties the action and puts its first default binding back in slot 0.
    void resetAction(ShortcutAction action);
    void clearAll();
    // Writes the default of every action that has one into `slot`, leaving the other slots alone.
    void applyDefaultsToSlot(int slot);

    // How many bindings in the whole map are this input (1 means no conflict).
    int usesOf(const InputBinding& binding) const;
    // The actions bound to this key and modifiers, exactly.
    std::vector<ShortcutAction> matchKey(int qtKey, int modifiers) const;

    QJsonObject toJson() const;
    // Actions missing from the object keep their defaults.
    static ShortcutMap fromJson(const QJsonObject& object);

    bool operator==(const ShortcutMap& other) const = default;

private:
    std::array<Slots, kShortcutCount> m_actions{};
};

// The default bindings of an action, in slot order.
std::vector<InputBinding> defaultBindings(ShortcutAction action);
