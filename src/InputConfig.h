#pragma once

#include <QJsonObject>
#include <QString>
#include <array>
#include <map>
#include <optional>

#include "InputBinding.h"
#include "SystemInfo.h"

// Bindings for every action of every input group, up to kSlots per action.
class InputConfig {
public:
    static constexpr int kSlots = 4;
    static constexpr int kFileVersion = 1;
    using Slots = std::array<InputBinding, kSlots>;

    // Keyboard defaults, in the second slot like the first column layout of the settings table.
    static InputConfig defaults();

    const Slots& bindings(InputGroup group, PadButton button) const;
    // Binds `binding` to the slot. When another action of the group already uses the same physical input,
    // it loses it and its button is returned so the caller can tell the user.
    std::optional<PadButton> assign(InputGroup group, PadButton button, int slot, const InputBinding& binding);
    void clearSlot(InputGroup group, PadButton button, int slot);
    void clearAction(InputGroup group, PadButton button);
    void resetGroup(InputGroup group);

    QJsonObject toJson() const;
    // Groups or actions missing from the object keep their defaults.
    static InputConfig fromJson(const QJsonObject& object, QString* error = nullptr);

    // A missing file is not an error: the defaults are returned.
    static InputConfig load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    bool operator==(const InputConfig& other) const = default;

private:
    using Actions = std::array<Slots, kPadButtonCount>;
    std::map<InputGroup, Actions> m_groups;
};
