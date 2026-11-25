#include "Shortcuts.h"

#include <QJsonArray>
#include <Qt>

namespace {

constexpr int kCtrl = Qt::ControlModifier;
constexpr int kShift = Qt::ShiftModifier;

QString quickName(const char* prefix, int slot) { return QString("%1%2").arg(prefix).arg(slot); }

struct DefaultKey {
    int key;
    int modifiers;
};

// Same layout as snes9xrd where the action exists in both, so the twins feel alike.
std::vector<DefaultKey> defaultKeys(ShortcutAction action)
{
    const int index = static_cast<int>(action);
    const int quickSave0 = static_cast<int>(ShortcutAction::QuickSave0);
    const int quickLoad0 = static_cast<int>(ShortcutAction::QuickLoad0);
    if (index >= quickSave0 && index < quickSave0 + kQuickSlots)
        return {{Qt::Key_F1 + (index - quickSave0), kShift}};
    if (index >= quickLoad0 && index < quickLoad0 + kQuickSlots)
        return {{Qt::Key_F1 + (index - quickLoad0), 0}};

    switch (action) {
    case ShortcutAction::OpenRom: return {{Qt::Key_O, kCtrl}};
    case ShortcutAction::FastForward: return {{Qt::Key_Tab, 0}};
    case ShortcutAction::ToggleFastForward: return {{Qt::Key_QuoteLeft, 0}};
    case ShortcutAction::PauseContinue: return {{Qt::Key_P, 0}};
    case ShortcutAction::Reset: return {{Qt::Key_R, kCtrl}};
    case ShortcutAction::Quit: return {{Qt::Key_Q, kCtrl}};
    case ShortcutAction::ToggleFullscreen: return {{Qt::Key_F11, 0}};
    case ShortcutAction::Screenshot: return {{Qt::Key_F12, 0}};
    case ShortcutAction::SaveState: return {{Qt::Key_S, kCtrl}};
    case ShortcutAction::LoadState: return {{Qt::Key_L, kCtrl}};
    case ShortcutAction::UndoState: return {{Qt::Key_U, kCtrl}};
    // Ctrl++ needs Shift on most layouts, where Qt reports Ctrl+Shift+Plus; Ctrl+= is the same key without Shift.
    case ShortcutAction::IncreaseSlot: return {{Qt::Key_Plus, kCtrl}, {Qt::Key_Equal, kCtrl}};
    case ShortcutAction::DecreaseSlot: return {{Qt::Key_Minus, kCtrl}};
    case ShortcutAction::Rewind: return {{Qt::Key_Y, 0}};
    // Not "6": that is player 2's default Z button.
    case ShortcutAction::SwapControllers: return {{Qt::Key_6, kCtrl}};
    case ShortcutAction::SpeedUp: return {{Qt::Key_Equal, 0}};
    case ShortcutAction::SpeedDown: return {{Qt::Key_Minus, 0}};
    case ShortcutAction::FrameSkipUp: return {{Qt::Key_Plus, kShift}};
    case ShortcutAction::FrameSkipDown: return {{Qt::Key_Minus, kShift}, {Qt::Key_Underscore, kShift}};
    case ShortcutAction::FrameAdvance: return {{Qt::Key_Backslash, 0}};
    case ShortcutAction::VolumeDown: return {{Qt::Key_PageDown, kCtrl}};
    case ShortcutAction::VolumeUp: return {{Qt::Key_PageUp, kCtrl}};
    default: return {};
    }
}

}

const std::vector<ShortcutDef>& allShortcuts()
{
    static const std::vector<ShortcutDef> shortcuts = [] {
        std::vector<ShortcutDef> list = {
            {ShortcutAction::OpenRom, "OpenROM", "Open ROM", false},
            {ShortcutAction::FastForward, "FastForward", "Enable Fast-Forward", true},
            {ShortcutAction::ToggleFastForward, "ToggleFastForward", "Toggle Fast-Forward", false},
            {ShortcutAction::PauseContinue, "PauseContinue", "Pause/Continue", false},
            {ShortcutAction::Reset, "SoftReset", "Reset", false},
            {ShortcutAction::PowerCycle, "Reset", "Power Cycle Console", false},
            {ShortcutAction::Quit, "Quit", "Quit", false},
            {ShortcutAction::ToggleFullscreen, "ToggleFullscreen", "Toggle Fullscreen", false},
            {ShortcutAction::Screenshot, "Screenshot", "Save Screenshot", false},
            {ShortcutAction::SaveState, "SaveState", "Save State to Current Slot", false},
            {ShortcutAction::LoadState, "LoadState", "Load State from Current Slot", false},
            {ShortcutAction::UndoState, "UndoState", "Undo State", false},
            {ShortcutAction::IncreaseSlot, "IncreaseSlot", "Increase Current Save Slot", false},
            {ShortcutAction::DecreaseSlot, "DecreaseSlot", "Decrease Current Save Slot", false},
        };
        for (int slot = 0; slot < kQuickSlots; ++slot)
            list.push_back({quickSaveAction(slot), quickName("QuickSave", slot), QString("Quick Save Slot %1").arg(slot), false});
        for (int slot = 0; slot < kQuickSlots; ++slot)
            list.push_back({quickLoadAction(slot), quickName("QuickLoad", slot), QString("Quick Load Slot %1").arg(slot), false});
        list.push_back({ShortcutAction::Rewind, "Rewind", "Rewind", true});
        list.push_back({ShortcutAction::SwapControllers, "SwapControllers1and2", "Swap Controllers 1 and 2", false});
        list.push_back({ShortcutAction::SpeedUp, "SpeedUp", "Speed Up", false});
        list.push_back({ShortcutAction::SpeedDown, "SpeedDown", "Speed Down", false});
        list.push_back({ShortcutAction::FrameSkipUp, "FrameSkipUp", "Increase Frame Skip", false});
        list.push_back({ShortcutAction::FrameSkipDown, "FrameSkipDown", "Decrease Frame Skip", false});
        list.push_back({ShortcutAction::FrameAdvance, "FrameAdvance", "Frame Advance", false});
        list.push_back({ShortcutAction::Mute, "ToggleMute", "Mute Audio", false});
        list.push_back({ShortcutAction::VolumeDown, "VolumeDown", "Volume Down", false});
        list.push_back({ShortcutAction::VolumeUp, "VolumeUp", "Volume Up", false});
        list.push_back({ShortcutAction::NextDisc, "NextDisc", "Switch to Next Disc", false});
        list.push_back({ShortcutAction::PreviousDisc, "PreviousDisc", "Switch to Previous Disc", false});
        return list;
    }();
    return shortcuts;
}

const ShortcutDef& shortcutDef(ShortcutAction id)
{
    return allShortcuts()[static_cast<std::size_t>(id)];
}

std::vector<InputBinding> defaultBindings(ShortcutAction action)
{
    std::vector<InputBinding> bindings;
    for (const DefaultKey& key : defaultKeys(action))
        bindings.push_back(InputBinding::key(key.key, 0, key.modifiers));
    return bindings;
}

ShortcutMap ShortcutMap::defaults()
{
    ShortcutMap map;
    for (const ShortcutDef& def : allShortcuts())
        map.resetAction(def.id);
    return map;
}

void ShortcutMap::resetAction(ShortcutAction action)
{
    clearAction(action);
    const std::vector<InputBinding> defaults = defaultBindings(action);
    for (std::size_t i = 0; i < defaults.size() && i < kSlots; ++i)
        set(action, static_cast<int>(i), defaults[i]);
}

void ShortcutMap::clearAll()
{
    for (Slots& row : m_actions)
        row = {};
}

void ShortcutMap::applyDefaultsToSlot(int slot)
{
    for (const ShortcutDef& def : allShortcuts()) {
        const std::vector<InputBinding> defaults = defaultBindings(def.id);
        if (!defaults.empty())
            set(def.id, slot, defaults.front());
    }
}

int ShortcutMap::usesOf(const InputBinding& binding) const
{
    if (binding.isNone())
        return 0;
    int count = 0;
    for (const Slots& row : m_actions)
        for (const InputBinding& other : row)
            if (other.sameSource(binding))
                ++count;
    return count;
}

std::vector<ShortcutAction> ShortcutMap::matchKey(int qtKey, int modifiers) const
{
    std::vector<ShortcutAction> matches;
    for (const ShortcutDef& def : allShortcuts()) {
        for (const InputBinding& binding : bindings(def.id)) {
            if (binding.type == InputBinding::Type::Key && binding.code == qtKey && binding.modifiers == (modifiers & InputBinding::kModifierMask)) {
                matches.push_back(def.id);
                break;
            }
        }
    }
    return matches;
}

QJsonObject ShortcutMap::toJson() const
{
    QJsonObject object;
    for (const ShortcutDef& def : allShortcuts()) {
        QJsonArray row;
        for (const InputBinding& binding : bindings(def.id))
            row.append(binding.toJson());
        object.insert(def.key, row);
    }
    return object;
}

ShortcutMap ShortcutMap::fromJson(const QJsonObject& object)
{
    ShortcutMap map = defaults();
    for (const ShortcutDef& def : allShortcuts()) {
        if (!object.value(def.key).isArray())
            continue;
        const QJsonArray saved = object.value(def.key).toArray();
        Slots row{};
        for (int i = 0; i < kSlots && i < saved.size(); ++i)
            row[i] = InputBinding::fromJson(saved[i].toObject());
        map.m_actions[static_cast<std::size_t>(def.id)] = row;
    }
    return map;
}
