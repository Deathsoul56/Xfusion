#include "BindingIcons.h"

#include "Icons.h"

QIcon bindingIcon(const InputBinding& binding, const QColor& color)
{
    switch (binding.type) {
    case InputBinding::Type::Key:
        return themedIcon("keyboard", color);
    case InputBinding::Type::JoyButton:
    case InputBinding::Type::JoyHat:
    case InputBinding::Type::JoyAxis:
        return themedIcon("joypad", color);
    case InputBinding::Type::None:
        break;
    }
    return {};
}
