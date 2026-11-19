#include "InputConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <Qt>

namespace {

constexpr int kKeyboardSlot = 1;

QString buttonName(PadButton button)
{
    static const char* const names[] = {"up", "down", "left", "right", "a", "b", "c", "x", "y", "z", "start", "mode"};
    return names[static_cast<std::size_t>(button)];
}

struct DefaultKey {
    PadButton button;
    int qtKey;
    int windowsVirtualKey;
};

// Windows virtual keys: letters use their ASCII code, the rest are fixed values.
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkReturn = 0x0D;
constexpr int kVkBack = 0x08;

const std::map<InputGroup, std::vector<DefaultKey>>& defaultKeys()
{
    static const std::map<InputGroup, std::vector<DefaultKey>> keys = {
        {InputGroup::Genesis,
         {{PadButton::Up, Qt::Key_Up, kVkUp},
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
          {PadButton::Mode, Qt::Key_Backspace, kVkBack}}},
        {InputGroup::MasterSystem,
         {{PadButton::Up, Qt::Key_Up, kVkUp},
          {PadButton::Down, Qt::Key_Down, kVkDown},
          {PadButton::Left, Qt::Key_Left, kVkLeft},
          {PadButton::Right, Qt::Key_Right, kVkRight},
          {PadButton::B, Qt::Key_Z, 'Z'},
          {PadButton::C, Qt::Key_X, 'X'},
          {PadButton::Start, Qt::Key_Return, kVkReturn}}},
    };
    return keys;
}

}

InputConfig InputConfig::defaults()
{
    InputConfig config;
    for (const InputGroupDef& group : allInputGroups())
        config.resetGroup(group.id);
    return config;
}

void InputConfig::resetGroup(InputGroup group)
{
    Actions actions{};
    for (const DefaultKey& key : defaultKeys().at(group))
        actions[static_cast<std::size_t>(key.button)][kKeyboardSlot] = InputBinding::key(key.qtKey, key.windowsVirtualKey);
    m_groups[group] = actions;
}

const InputConfig::Slots& InputConfig::bindings(InputGroup group, PadButton button) const
{
    return m_groups.at(group)[static_cast<std::size_t>(button)];
}

std::optional<PadButton> InputConfig::assign(InputGroup group, PadButton button, int slot, const InputBinding& binding)
{
    std::optional<PadButton> displaced;
    Actions& actions = m_groups.at(group);
    if (!binding.isNone()) {
        for (const InputAction& action : inputGroupDef(group).actions) {
            Slots& row = actions[static_cast<std::size_t>(action.button)];
            for (int i = 0; i < kSlots; ++i) {
                if (action.button == button && i == slot)
                    continue;
                if (!row[i].sameSource(binding))
                    continue;
                row[i] = {};
                if (action.button != button)
                    displaced = action.button;
            }
        }
    }
    actions[static_cast<std::size_t>(button)][slot] = binding;
    return displaced;
}

void InputConfig::clearSlot(InputGroup group, PadButton button, int slot)
{
    m_groups.at(group)[static_cast<std::size_t>(button)][slot] = {};
}

void InputConfig::clearAction(InputGroup group, PadButton button)
{
    m_groups.at(group)[static_cast<std::size_t>(button)] = {};
}

QJsonObject InputConfig::toJson() const
{
    QJsonObject groups;
    for (const InputGroupDef& group : allInputGroups()) {
        QJsonObject actions;
        for (const InputAction& action : group.actions) {
            QJsonArray row;
            for (const InputBinding& binding : bindings(group.id, action.button))
                row.append(binding.toJson());
            actions.insert(buttonName(action.button), row);
        }
        groups.insert(group.key, actions);
    }
    return {{"version", kFileVersion}, {"groups", groups}};
}

InputConfig InputConfig::fromJson(const QJsonObject& object, QString* error)
{
    InputConfig config = defaults();
    if (object.value("version").toInt() != kFileVersion) {
        if (error)
            *error = QString("Unsupported input configuration version %1").arg(object.value("version").toInt());
        return config;
    }

    const QJsonObject groups = object.value("groups").toObject();
    for (const InputGroupDef& group : allInputGroups()) {
        if (!groups.contains(group.key))
            continue;
        const QJsonObject actions = groups.value(group.key).toObject();
        for (const InputAction& action : group.actions) {
            if (!actions.contains(buttonName(action.button)))
                continue;
            const QJsonArray saved = actions.value(buttonName(action.button)).toArray();
            Slots row{};
            for (int i = 0; i < kSlots && i < saved.size(); ++i)
                row[i] = InputBinding::fromJson(saved[i].toObject());
            config.m_groups.at(group.id)[static_cast<std::size_t>(action.button)] = row;
        }
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
