#include "InputConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <Qt>
#include <functional>
#include <vector>

namespace {

constexpr int kKeyboardSlot = 1;

QString buttonName(PadButton button)
{
    static const char* const names[] = {"up", "down", "left", "right", "a", "b", "c", "x", "y", "z", "start", "mode",
                                        "turbo_a", "turbo_b", "turbo_c", "turbo_x", "turbo_y", "turbo_z"};
    return names[static_cast<std::size_t>(button)];
}

struct DefaultKey {
    PadButton button;
    int qtKey;
    int windowsVirtualKey;
};

// Windows virtual keys: letters and digits use their ASCII code, the rest are fixed values.
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkReturn = 0x0D;
constexpr int kVkBack = 0x08;

using PlayerKeys = std::array<std::vector<DefaultKey>, InputConfig::kPlayers>;

// Player 1 uses the left hand and the arrows; player 2 gets I J K L and the number row, so two people can share
// a keyboard without any key in common (and none of them is a default hotkey).
const std::map<InputGroup, PlayerKeys>& defaultKeys()
{
    static const std::map<InputGroup, PlayerKeys> keys = {
        {InputGroup::Genesis,
         {{{{PadButton::Up, Qt::Key_Up, kVkUp},
            {PadButton::Down, Qt::Key_Down, kVkDown},
            {PadButton::Left, Qt::Key_Left, kVkLeft},
            {PadButton::Right, Qt::Key_Right, kVkRight},
            {PadButton::A, Qt::Key_Z, 'Z'},
            {PadButton::B, Qt::Key_X, 'X'},
            {PadButton::C, Qt::Key_C, 'C'},
            {PadButton::X, Qt::Key_A, 'A'},
            {PadButton::Y, Qt::Key_S, 'S'},
            {PadButton::Z, Qt::Key_D, 'D'},
            {PadButton::Start, Qt::Key_Return, kVkReturn},
            {PadButton::Mode, Qt::Key_Backspace, kVkBack}},
           {{PadButton::Up, Qt::Key_I, 'I'},
            {PadButton::Down, Qt::Key_K, 'K'},
            {PadButton::Left, Qt::Key_J, 'J'},
            {PadButton::Right, Qt::Key_L, 'L'},
            {PadButton::A, Qt::Key_7, '7'},
            {PadButton::B, Qt::Key_8, '8'},
            {PadButton::C, Qt::Key_9, '9'},
            {PadButton::X, Qt::Key_4, '4'},
            {PadButton::Y, Qt::Key_5, '5'},
            {PadButton::Z, Qt::Key_6, '6'},
            {PadButton::Start, Qt::Key_0, '0'},
            {PadButton::Mode, Qt::Key_3, '3'}}}}},
        {InputGroup::MasterSystem,
         {{{{PadButton::Up, Qt::Key_Up, kVkUp},
            {PadButton::Down, Qt::Key_Down, kVkDown},
            {PadButton::Left, Qt::Key_Left, kVkLeft},
            {PadButton::Right, Qt::Key_Right, kVkRight},
            {PadButton::B, Qt::Key_Z, 'Z'},
            {PadButton::C, Qt::Key_X, 'X'},
            {PadButton::Start, Qt::Key_Return, kVkReturn}},
           {{PadButton::Up, Qt::Key_I, 'I'},
            {PadButton::Down, Qt::Key_K, 'K'},
            {PadButton::Left, Qt::Key_J, 'J'},
            {PadButton::Right, Qt::Key_L, 'L'},
            {PadButton::B, Qt::Key_7, '7'},
            {PadButton::C, Qt::Key_8, '8'},
            {PadButton::Start, Qt::Key_0, '0'}}}}},
    };
    return keys;
}

QJsonObject actionsToJson(const InputGroupDef& group, const std::function<const InputConfig::Slots&(PadButton)>& slotsOf)
{
    QJsonObject actions;
    for (const InputAction& action : group.actions) {
        QJsonArray row;
        for (const InputBinding& binding : slotsOf(action.button))
            row.append(binding.toJson());
        actions.insert(buttonName(action.button), row);
    }
    return actions;
}

}

InputConfig InputConfig::defaults()
{
    InputConfig config;
    for (const InputGroupDef& group : allInputGroups())
        config.resetGroup(group.id);
    return config;
}

void InputConfig::resetPlayer(InputGroup group, int player)
{
    Actions actions{};
    for (const DefaultKey& key : defaultKeys().at(group)[static_cast<std::size_t>(player)])
        actions[static_cast<std::size_t>(key.button)][kKeyboardSlot] = InputBinding::key(key.qtKey, key.windowsVirtualKey);
    m_groups[group][static_cast<std::size_t>(player)] = actions;
}

void InputConfig::applyDefaultKeyboard(InputGroup group, int player, int slot)
{
    for (const DefaultKey& key : defaultKeys().at(group)[static_cast<std::size_t>(player)])
        m_groups.at(group)[static_cast<std::size_t>(player)][static_cast<std::size_t>(key.button)][slot] =
            InputBinding::key(key.qtKey, key.windowsVirtualKey);
}

void InputConfig::clearPlayer(InputGroup group, int player)
{
    m_groups.at(group)[static_cast<std::size_t>(player)] = {};
}

void InputConfig::resetGroup(InputGroup group)
{
    for (int player = 0; player < kPlayers; ++player)
        resetPlayer(group, player);
    if (group == InputGroup::Genesis)
        m_padType = PadType::SixButton;
}

const InputConfig::Slots& InputConfig::bindings(InputGroup group, int player, PadButton button) const
{
    return m_groups.at(group)[static_cast<std::size_t>(player)][static_cast<std::size_t>(button)];
}

std::optional<InputConfig::Displaced> InputConfig::assign(InputGroup group, int player, PadButton button, int slot,
                                                          const InputBinding& binding)
{
    std::optional<Displaced> displaced;
    PlayerActions& players = m_groups.at(group);
    if (!binding.isNone()) {
        for (int other = 0; other < kPlayers; ++other) {
            if (other != player && binding.type != InputBinding::Type::Key)
                continue;
            for (const InputAction& action : inputGroupDef(group).actions) {
                Slots& row = players[static_cast<std::size_t>(other)][static_cast<std::size_t>(action.button)];
                for (int i = 0; i < kSlots; ++i) {
                    if (other == player && action.button == button && i == slot)
                        continue;
                    if (!row[i].sameSource(binding))
                        continue;
                    row[i] = {};
                    if (other != player || action.button != button)
                        displaced = Displaced{other, action.button};
                }
            }
        }
    }
    players[static_cast<std::size_t>(player)][static_cast<std::size_t>(button)][slot] = binding;
    return displaced;
}

void InputConfig::clearSlot(InputGroup group, int player, PadButton button, int slot)
{
    m_groups.at(group)[static_cast<std::size_t>(player)][static_cast<std::size_t>(button)][slot] = {};
}

void InputConfig::clearAction(InputGroup group, int player, PadButton button)
{
    m_groups.at(group)[static_cast<std::size_t>(player)][static_cast<std::size_t>(button)] = {};
}

QJsonObject InputConfig::toJson() const
{
    QJsonObject groups;
    for (const InputGroupDef& group : allInputGroups()) {
        QJsonArray players;
        for (int player = 0; player < kPlayers; ++player)
            players.append(actionsToJson(group, [&](PadButton button) -> const Slots& { return bindings(group.id, player, button); }));
        groups.insert(group.key, QJsonObject{{"players", players}});
    }
    QJsonArray devices;
    for (const QString& name : m_devices)
        devices.append(name);
    return {{"version", kFileVersion},
            {"genesis_pad", m_padType == PadType::ThreeButton ? 3 : 6},
            {"ports", m_portMode == PortMode::OneController ? "one" : "two"},
            {"devices", devices},
            {"groups", groups}};
}

InputConfig InputConfig::fromJson(const QJsonObject& object, QString* error)
{
    InputConfig config = defaults();
    const int version = object.value("version").toInt();
    if (version != 1 && version != kFileVersion) {
        if (error)
            *error = QString("Unsupported input configuration version %1").arg(version);
        return config;
    }

    if (object.value("genesis_pad").toInt() == 3)
        config.m_padType = PadType::ThreeButton;
    if (object.value("ports").toString() == "one")
        config.m_portMode = PortMode::OneController;

    const QJsonArray devices = object.value("devices").toArray();
    for (int player = 0; player < kPlayers && player < devices.size(); ++player)
        config.m_devices[static_cast<std::size_t>(player)] = devices[player].toString();

    const auto readActions = [&](const InputGroupDef& group, int player, const QJsonObject& actions) {
        for (const InputAction& action : group.actions) {
            if (!actions.contains(buttonName(action.button)))
                continue;
            const QJsonArray saved = actions.value(buttonName(action.button)).toArray();
            Slots row{};
            for (int i = 0; i < kSlots && i < saved.size(); ++i)
                row[i] = InputBinding::fromJson(saved[i].toObject());
            config.m_groups.at(group.id)[static_cast<std::size_t>(player)][static_cast<std::size_t>(action.button)] = row;
        }
    };

    const QJsonObject groups = object.value("groups").toObject();
    for (const InputGroupDef& group : allInputGroups()) {
        if (!groups.contains(group.key))
            continue;
        const QJsonObject saved = groups.value(group.key).toObject();
        if (version == 1) {
            readActions(group, 0, saved);
            continue;
        }
        const QJsonArray players = saved.value("players").toArray();
        for (int player = 0; player < kPlayers && player < players.size(); ++player)
            readActions(group, player, players[player].toObject());
    }
    return config;
}

InputConfig InputConfig::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.exists())
        return defaults();
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString("Cannot read %1: %2").arg(path, file.errorString());
        return defaults();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error)
            *error = QString("Invalid input configuration %1: %2").arg(path, parseError.errorString());
        return defaults();
    }
    return fromJson(document.object(), error);
}

bool InputConfig::save(const QString& path, QString* error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
