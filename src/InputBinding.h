#pragma once

#include <QJsonObject>
#include <QString>

// One physical input that can be bound to a pad button. Kept free of SDL so it can be tested and stored alone.
struct InputBinding {
    enum class Type { None, Key, JoyButton, JoyHat, JoyAxis };

    // Same values as SDL's hat directions.
    static constexpr int kHatUp = 1;
    static constexpr int kHatRight = 2;
    static constexpr int kHatDown = 4;
    static constexpr int kHatLeft = 8;

    Type type = Type::None;
    // Key: Qt::Key. JoyButton: button index. JoyHat: hat index. JoyAxis: axis index.
    int code = 0;
    // Key: Windows virtual key, or 0 when unknown. JoyHat: direction bit. JoyAxis: +1 or -1.
    int value = 0;
    // Key only: Qt::KeyboardModifier bits (Shift, Ctrl, Alt, Meta). Pad bindings leave it 0 and ignore modifiers;
    // shortcuts match them exactly.
    int modifiers = 0;

    static constexpr int kModifierMask = 0x02000000 | 0x04000000 | 0x08000000 | 0x10000000;

    static InputBinding key(int qtKey, int windowsVirtualKey, int modifiers = 0);
    static InputBinding joyButton(int button);
    static InputBinding joyHat(int hat, int direction);
    static InputBinding joyAxis(int axis, int sign);

    bool isNone() const { return type == Type::None; }
    // Whether both refer to the same physical input; a key is identified by its Qt code alone.
    bool sameSource(const InputBinding& other) const;
    bool operator==(const InputBinding& other) const = default;

    QString displayName() const;
    QJsonObject toJson() const;
    static InputBinding fromJson(const QJsonObject& object);
};
