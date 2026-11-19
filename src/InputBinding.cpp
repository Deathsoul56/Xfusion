#include "InputBinding.h"

#include <QKeySequence>

InputBinding InputBinding::key(int qtKey, int windowsVirtualKey)
{
    return {Type::Key, qtKey, windowsVirtualKey};
}

InputBinding InputBinding::joyButton(int button)
{
    return {Type::JoyButton, button, 0};
}

InputBinding InputBinding::joyHat(int hat, int direction)
{
    return {Type::JoyHat, hat, direction};
}

InputBinding InputBinding::joyAxis(int axis, int sign)
{
    return {Type::JoyAxis, axis, sign < 0 ? -1 : 1};
}

bool InputBinding::sameSource(const InputBinding& other) const
{
    if (type != other.type || isNone())
        return false;
    if (type == Type::Key || type == Type::JoyButton)
        return code == other.code;
    return code == other.code && value == other.value;
}

QString InputBinding::displayName() const
{
    switch (type) {
    case Type::Key:
        return QKeySequence(code).toString(QKeySequence::NativeText);
    case Type::JoyButton:
        return QString("Button %1").arg(code);
    case Type::JoyHat: {
        const char* direction = value == kHatUp ? "Up" : value == kHatDown ? "Down" : value == kHatLeft ? "Left" : "Right";
        return QString("Hat %1 %2").arg(code).arg(direction);
    }
    case Type::JoyAxis:
        return QString("Axis %1 %2").arg(code).arg(value < 0 ? "-" : "+");
    case Type::None:
        break;
    }
    return "None";
}

QJsonObject InputBinding::toJson() const
{
    static const char* const names[] = {"none", "key", "button", "hat", "axis"};
    return {{"type", names[static_cast<int>(type)]}, {"code", code}, {"value", value}};
}

InputBinding InputBinding::fromJson(const QJsonObject& object)
{
    const QString name = object.value("type").toString();
    InputBinding binding;
    if (name == "key")
        binding.type = Type::Key;
    else if (name == "button")
        binding.type = Type::JoyButton;
    else if (name == "hat")
        binding.type = Type::JoyHat;
    else if (name == "axis")
        binding.type = Type::JoyAxis;
    else
        return {};
    binding.code = object.value("code").toInt();
    binding.value = object.value("value").toInt();
    return binding;
}
