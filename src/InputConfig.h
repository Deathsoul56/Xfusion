#pragma once

#include <QJsonObject>
#include <QString>
#include <array>
#include <map>
#include <optional>

#include "InputBinding.h"
#include "SystemInfo.h"

// Bindings for every action of every input group and player, up to kSlots per action, and the controller
// each player uses.
class InputConfig {
public:
    static constexpr int kSlots = 4;
    static constexpr int kPlayers = 2;
    // Version 1 had a single player; its groups hold the actions of player 1 directly.
    static constexpr int kFileVersion = 2;
    using Slots = std::array<InputBinding, kSlots>;

    // The action that lost an input to a new assignment.
    struct Displaced {
        int player;
        PadButton button;
        bool operator==(const Displaced& other) const = default;
    };

    // Keyboard defaults, in the second slot like the first column layout of the settings table.
    static InputConfig defaults();

    const Slots& bindings(InputGroup group, int player, PadButton button) const;
    // Binds `binding` to the slot. A key can only drive one action of the group, whichever player it belongs to;
    // a controller input only one action of the same player, because each player has a controller of their own.
    // The action that lost the input is returned so the caller can tell the user.
    std::optional<Displaced> assign(InputGroup group, int player, PadButton button, int slot, const InputBinding& binding);
    void clearSlot(InputGroup group, int player, PadButton button, int slot);
    void clearAction(InputGroup group, int player, PadButton button);
    void resetPlayer(InputGroup group, int player);
    // Both players and, for the Genesis group, the pad type.
    void resetGroup(InputGroup group);

    // How many controllers are plugged in. With one, the second port is empty for the game.
    enum class PortMode { OneController, TwoControllers };
    PortMode portMode() const { return m_portMode; }
    void setPortMode(PortMode mode) { m_portMode = mode; }

    // Puts the default keyboard bindings of `player` into one slot only, leaving the other slots alone.
    void applyDefaultKeyboard(InputGroup group, int player, int slot);
    // Empties every binding of the player (all groups stay as they are).
    void clearPlayer(InputGroup group, int player);

    // Which Genesis-family pad is plugged in; the MasterSystem group has a single pad type.
    PadType padType() const { return m_padType; }
    void setPadType(PadType type) { m_padType = type; }

    // The controller a player uses, by the name SDL reports. Empty means automatic: the connected controllers
    // that no player picked by name, in the order they were plugged in.
    const QString& device(int player) const { return m_devices[static_cast<std::size_t>(player)]; }
    void setDevice(int player, const QString& name) { m_devices[static_cast<std::size_t>(player)] = name; }

    QJsonObject toJson() const;
    // Groups or actions missing from the object keep their defaults.
    static InputConfig fromJson(const QJsonObject& object, QString* error = nullptr);

    // A missing file is not an error: the defaults are returned.
    static InputConfig load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    bool operator==(const InputConfig& other) const = default;

private:
    using Actions = std::array<Slots, kPadButtonCount>;
    using PlayerActions = std::array<Actions, kPlayers>;
    std::map<InputGroup, PlayerActions> m_groups;
    std::array<QString, kPlayers> m_devices;
    PadType m_padType = PadType::SixButton;
    PortMode m_portMode = PortMode::TwoControllers;
};
