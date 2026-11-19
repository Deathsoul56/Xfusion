#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <SDL3/SDL.h>

#include "InputBinding.h"
#include "InputConfig.h"
#include "InputManager.h"
#include "SystemInfo.h"

namespace {

// Detaches the virtual device even when a check fails halfway through a test.
class VirtualJoystick {
public:
    VirtualJoystick(SDL_JoystickType type, int axes, int buttons, int hats)
    {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = static_cast<Uint16>(type);
        desc.naxes = static_cast<Uint16>(axes);
        desc.nbuttons = static_cast<Uint16>(buttons);
        desc.nhats = static_cast<Uint16>(hats);
        desc.name = "Xfusion test device";
        m_id = SDL_AttachVirtualJoystick(&desc);
        if (m_id)
            m_handle = SDL_OpenJoystick(m_id);
    }
    ~VirtualJoystick()
    {
        if (m_handle)
            SDL_CloseJoystick(m_handle);
        if (m_id)
            SDL_DetachVirtualJoystick(m_id);
    }
    VirtualJoystick(const VirtualJoystick&) = delete;
    VirtualJoystick& operator=(const VirtualJoystick&) = delete;

    explicit operator bool() const { return m_handle != nullptr; }
    operator SDL_Joystick*() const { return m_handle; }

private:
    SDL_JoystickID m_id = 0;
    SDL_Joystick* m_handle = nullptr;
};

}

class InputTests : public QObject {
    Q_OBJECT

private slots:
    // Physical controllers would make the device counts and "no controller" checks depend on the machine.
    void initTestCase()
    {
        for (const char* hint : {"SDL_XINPUT_ENABLED", "SDL_JOYSTICK_RAWINPUT", "SDL_JOYSTICK_HIDAPI", "SDL_JOYSTICK_WGI",
                                 "SDL_JOYSTICK_DIRECTINPUT"})
            SDL_SetHint(hint, "0");
    }
    void systemsShareInputGroups()
    {
        QCOMPARE(systemDef(System::Genesis).inputGroup, InputGroup::Genesis);
        QCOMPARE(systemDef(System::SegaCD).inputGroup, InputGroup::Genesis);
        QCOMPARE(systemDef(System::Sega32X).inputGroup, InputGroup::Genesis);
        QCOMPARE(systemDef(System::MasterSystem).inputGroup, InputGroup::MasterSystem);
        QCOMPARE(systemDef(System::GameGear).inputGroup, InputGroup::MasterSystem);
        QCOMPARE(systemDef(System::SG1000).inputGroup, InputGroup::MasterSystem);
        QCOMPARE(inputGroupTitle(InputGroup::Genesis), QString("Genesis / Sega CD / 32X"));
        QCOMPARE(inputGroupTitle(InputGroup::MasterSystem), QString("SG-1000 / Master System / Game Gear"));
        QCOMPARE(inputGroupDef(InputGroup::Genesis).actions.size(), size_t(12));
        QCOMPARE(inputGroupDef(InputGroup::MasterSystem).actions.size(), size_t(7));
    }

    void defaultsBindTheKeyboardOnly()
    {
        const InputConfig config = InputConfig::defaults();
        const auto& genesisA = config.bindings(InputGroup::Genesis, PadButton::A);
        QVERIFY(genesisA[0].isNone());
        QCOMPARE(genesisA[1], InputBinding::key(Qt::Key_Z, 'Z'));
        QCOMPARE(config.bindings(InputGroup::MasterSystem, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
        QCOMPARE(config.bindings(InputGroup::MasterSystem, PadButton::C)[1], InputBinding::key(Qt::Key_X, 'X'));
    }

    void assigningAnInputTakesItFromTheOtherAction()
    {
        InputConfig config = InputConfig::defaults();
        const auto displaced = config.assign(InputGroup::Genesis, PadButton::B, 0, InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(displaced.has_value());
        QCOMPARE(*displaced, PadButton::A);
        QVERIFY(config.bindings(InputGroup::Genesis, PadButton::A)[1].isNone());
        QCOMPARE(config.bindings(InputGroup::Genesis, PadButton::B)[0], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void assigningToTheSameActionReportsNoConflict()
    {
        InputConfig config = InputConfig::defaults();
        const auto displaced = config.assign(InputGroup::Genesis, PadButton::A, 0, InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(!displaced.has_value());
        QVERIFY(config.bindings(InputGroup::Genesis, PadButton::A)[1].isNone());
    }

    void bindingsOfOneGroupDoNotAffectTheOther()
    {
        InputConfig config = InputConfig::defaults();
        config.clearAction(InputGroup::Genesis, PadButton::A);
        QCOMPARE(config.bindings(InputGroup::MasterSystem, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
        config.assign(InputGroup::MasterSystem, PadButton::B, 0, InputBinding::joyButton(3));
        QVERIFY(config.bindings(InputGroup::Genesis, PadButton::B)[0].isNone());
    }

    void resetGroupRestoresDefaults()
    {
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, PadButton::Start, 0, InputBinding::joyButton(7));
        config.clearAction(InputGroup::MasterSystem, PadButton::Start);
        config.resetGroup(InputGroup::Genesis);
        QVERIFY(config.bindings(InputGroup::Genesis, PadButton::Start)[0].isNone());
        QVERIFY(config.bindings(InputGroup::MasterSystem, PadButton::Start)[1].isNone());
    }

    void jsonRoundTrip()
    {
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, PadButton::C, 0, InputBinding::joyHat(0, InputBinding::kHatLeft));
        config.assign(InputGroup::Genesis, PadButton::C, 2, InputBinding::joyAxis(4, -1));
        QCOMPARE(InputConfig::fromJson(config.toJson()), config);
    }

    void unsupportedVersionFallsBackToDefaults()
    {
        QString error;
        const InputConfig config = InputConfig::fromJson({{"version", 99}}, &error);
        QCOMPARE(config, InputConfig::defaults());
        QVERIFY(!error.isEmpty());
    }

    void missingGroupsKeepTheirDefaults()
    {
        InputConfig custom = InputConfig::defaults();
        custom.assign(InputGroup::Genesis, PadButton::A, 0, InputBinding::joyButton(9));
        QJsonObject json = custom.toJson();
        QJsonObject groups = json.value("groups").toObject();
        groups.remove("master_system");
        json.insert("groups", groups);
        const InputConfig loaded = InputConfig::fromJson(json);
        QCOMPARE(loaded.bindings(InputGroup::Genesis, PadButton::A)[0], InputBinding::joyButton(9));
        QCOMPARE(loaded.bindings(InputGroup::MasterSystem, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void saveAndLoadThroughAFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("input.json");

        QString error;
        QCOMPARE(InputConfig::load(path, &error), InputConfig::defaults());
        QVERIFY(error.isEmpty());

        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::MasterSystem, PadButton::Start, 0, InputBinding::joyButton(6));
        QVERIFY(config.save(path, &error));
        QCOMPARE(InputConfig::load(path, &error), config);

        QFile corrupt(path);
        QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
        corrupt.write("{ not json");
        corrupt.close();
        QCOMPARE(InputConfig::load(path, &error), InputConfig::defaults());
        QVERIFY(!error.isEmpty());
    }

    void displayNames()
    {
        QCOMPARE(InputBinding::joyButton(3).displayName(), QString("Button 3"));
        QCOMPARE(InputBinding::joyHat(0, InputBinding::kHatUp).displayName(), QString("Hat 0 Up"));
        QCOMPARE(InputBinding::joyAxis(1, -1).displayName(), QString("Axis 1 -"));
        QCOMPARE(InputBinding().displayName(), QString("None"));
        QVERIFY(!InputBinding::key(Qt::Key_Up, 0x26).displayName().isEmpty());
    }

    void joystickInputsBecomePadButtons()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 4, 1);
        QVERIFY2(device, SDL_GetError());

        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, PadButton::A, 0, InputBinding::joyButton(2));
        config.assign(InputGroup::Genesis, PadButton::B, 0, InputBinding::joyAxis(1, 1));
        config.assign(InputGroup::Genesis, PadButton::Up, 0, InputBinding::joyHat(0, InputBinding::kHatUp));
        manager.setConfig(config);
        QCOMPARE(manager.controllerNames().size(), 1);

        const auto index = [](PadButton button) { return static_cast<std::size_t>(button); };
        auto state = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);

        SDL_SetJoystickVirtualButton(device, 2, true);
        SDL_SetJoystickVirtualAxis(device, 1, 30000);
        SDL_SetJoystickVirtualHat(device, 0, SDL_HAT_UP);
        state = manager.poll(InputGroup::Genesis, true);
        QVERIFY(state[index(PadButton::A)]);
        QVERIFY(state[index(PadButton::B)]);
        QVERIFY(state[index(PadButton::Up)]);
        QVERIFY(!state[index(PadButton::C)]);

        state = manager.poll(InputGroup::Genesis, false);
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);

        SDL_SetJoystickVirtualButton(device, 2, false);
        SDL_SetJoystickVirtualAxis(device, 1, 0);
        SDL_SetJoystickVirtualHat(device, 0, SDL_HAT_CENTERED);
        state = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);
    }

    void axisBelowTheThresholdIsIgnored()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 1, 0);
        QVERIFY2(device, SDL_GetError());
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, PadButton::Left, 0, InputBinding::joyAxis(0, -1));
        manager.setConfig(config);
        manager.controllerNames();

        SDL_SetJoystickVirtualAxis(device, 0, -8000);
        QVERIFY(!manager.poll(InputGroup::Genesis, true)[static_cast<std::size_t>(PadButton::Left)]);
        SDL_SetJoystickVirtualAxis(device, 0, -30000);
        QVERIFY(manager.poll(InputGroup::Genesis, true)[static_cast<std::size_t>(PadButton::Left)]);
    }

    void captureIgnoresInputsHeldBeforeItBegan()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 4, 0);
        QVERIFY2(device, SDL_GetError());
        manager.controllerNames();

        SDL_SetJoystickVirtualButton(device, 1, true);
        manager.beginCapture();
        QVERIFY(!manager.captureJoystick().has_value());

        SDL_SetJoystickVirtualButton(device, 3, true);
        const auto captured = manager.captureJoystick();
        QVERIFY(captured.has_value());
        QCOMPARE(*captured, InputBinding::joyButton(3));

        SDL_SetJoystickVirtualButton(device, 1, false);
        SDL_SetJoystickVirtualButton(device, 3, false);
        QVERIFY(!manager.captureJoystick().has_value());
        SDL_SetJoystickVirtualButton(device, 1, true);
        const auto again = manager.captureJoystick();
        QVERIFY(again.has_value());
        QCOMPARE(*again, InputBinding::joyButton(1));
    }

    void autoAssignUsesTheStandardGamepadLayout()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_GAMEPAD, SDL_GAMEPAD_AXIS_COUNT, SDL_GAMEPAD_BUTTON_COUNT, 0);
        QVERIFY2(device, SDL_GetError());
        manager.controllerNames();

        InputConfig config = InputConfig::defaults();
        QString message;
        const int assigned = manager.autoAssign(InputGroup::Genesis, config, &message);
        if (assigned == 0)
            QSKIP(qPrintable("SDL gave the virtual device no gamepad mapping: " + message));
        QVERIFY2(assigned >= 8, qPrintable(message));
        QVERIFY(!config.bindings(InputGroup::Genesis, PadButton::B)[0].isNone());
        QVERIFY(!config.bindings(InputGroup::Genesis, PadButton::Left)[2].isNone());
        // The keyboard column is left alone.
        QCOMPARE(config.bindings(InputGroup::Genesis, PadButton::A)[1], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void autoAssignWithoutControllersSaysSo()
    {
        InputManager manager;
        InputConfig config = InputConfig::defaults();
        QString message;
        QCOMPARE(manager.autoAssign(InputGroup::Genesis, config, &message), 0);
        QVERIFY(!message.isEmpty());
        QCOMPARE(config, InputConfig::defaults());
    }
};

QTEST_MAIN(InputTests)
#include "InputTests.moc"
