#include <QJsonArray>
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
    VirtualJoystick(SDL_JoystickType type, int axes, int buttons, int hats, const char* name = "Xfusion test device")
    {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = static_cast<Uint16>(type);
        desc.naxes = static_cast<Uint16>(axes);
        desc.nbuttons = static_cast<Uint16>(buttons);
        desc.nhats = static_cast<Uint16>(hats);
        desc.name = name;
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
        QCOMPARE(inputGroupDef(InputGroup::Genesis).actions.size(), size_t(18)); // 12 buttons and 6 turbo rows
        QCOMPARE(inputGroupDef(InputGroup::MasterSystem).actions.size(), size_t(9)); // 7 buttons and 2 turbo rows
    }

    void defaultsBindTheKeyboardOnly()
    {
        const InputConfig config = InputConfig::defaults();
        const auto& genesisA = config.bindings(InputGroup::Genesis, 0, PadButton::A);
        QVERIFY(genesisA[0].isNone());
        QCOMPARE(genesisA[1], InputBinding::key(Qt::Key_Z, 'Z'));
        QCOMPARE(config.bindings(InputGroup::MasterSystem, 0, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
        QCOMPARE(config.bindings(InputGroup::MasterSystem, 0, PadButton::C)[1], InputBinding::key(Qt::Key_X, 'X'));
    }

    void assigningAnInputTakesItFromTheOtherAction()
    {
        InputConfig config = InputConfig::defaults();
        const auto displaced = config.assign(InputGroup::Genesis, 0, PadButton::B, 0, InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(displaced.has_value());
        QCOMPARE(*displaced, (InputConfig::Displaced{0, PadButton::A}));
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1].isNone());
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::B)[0], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void assigningToTheSameActionReportsNoConflict()
    {
        InputConfig config = InputConfig::defaults();
        const auto displaced = config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(!displaced.has_value());
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1].isNone());
    }

    void bindingsOfOneGroupDoNotAffectTheOther()
    {
        InputConfig config = InputConfig::defaults();
        config.clearAction(InputGroup::Genesis, 0, PadButton::A);
        QCOMPARE(config.bindings(InputGroup::MasterSystem, 0, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
        config.assign(InputGroup::MasterSystem, 0, PadButton::B, 0, InputBinding::joyButton(3));
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::B)[0].isNone());
    }

    void resetGroupRestoresDefaults()
    {
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::Start, 0, InputBinding::joyButton(7));
        config.clearAction(InputGroup::MasterSystem, 0, PadButton::Start);
        config.resetGroup(InputGroup::Genesis);
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::Start)[0].isNone());
        QVERIFY(config.bindings(InputGroup::MasterSystem, 0, PadButton::Start)[1].isNone());
    }

    void jsonRoundTrip()
    {
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::C, 0, InputBinding::joyHat(0, InputBinding::kHatLeft));
        config.assign(InputGroup::Genesis, 0, PadButton::C, 2, InputBinding::joyAxis(4, -1));
        QCOMPARE(InputConfig::fromJson(config.toJson()), config);
    }

    void padTypeDefaultsToSixButtonAndHidesSixButtonActions()
    {
        QCOMPARE(InputConfig::defaults().padType(), PadType::SixButton);
        for (const InputAction& action : inputGroupDef(InputGroup::Genesis).actions) {
            QVERIFY(action.availableWith(PadType::SixButton));
            const bool sixOnly = action.button == PadButton::X || action.button == PadButton::Y
                || action.button == PadButton::Z || action.button == PadButton::Mode || action.button == PadButton::TurboX
                || action.button == PadButton::TurboY || action.button == PadButton::TurboZ;
            QCOMPARE(action.availableWith(PadType::ThreeButton), !sixOnly);
        }
        QVERIFY(inputGroupDef(InputGroup::Genesis).selectablePadType);
        QVERIFY(!inputGroupDef(InputGroup::MasterSystem).selectablePadType);
    }

    void padTypeIsSavedAndResetWithTheGenesisGroup()
    {
        InputConfig config = InputConfig::defaults();
        config.setPadType(PadType::ThreeButton);
        const InputConfig loaded = InputConfig::fromJson(config.toJson());
        QCOMPARE(loaded.padType(), PadType::ThreeButton);
        QCOMPARE(loaded, config);

        config.resetGroup(InputGroup::MasterSystem);
        QCOMPARE(config.padType(), PadType::ThreeButton);
        config.resetGroup(InputGroup::Genesis);
        QCOMPARE(config.padType(), PadType::SixButton);
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
        custom.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(9));
        QJsonObject json = custom.toJson();
        QJsonObject groups = json.value("groups").toObject();
        groups.remove("master_system");
        json.insert("groups", groups);
        const InputConfig loaded = InputConfig::fromJson(json);
        QCOMPARE(loaded.bindings(InputGroup::Genesis, 0, PadButton::A)[0], InputBinding::joyButton(9));
        QCOMPARE(loaded.bindings(InputGroup::MasterSystem, 0, PadButton::B)[1], InputBinding::key(Qt::Key_Z, 'Z'));
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
        config.assign(InputGroup::MasterSystem, 0, PadButton::Start, 0, InputBinding::joyButton(6));
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
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(2));
        config.assign(InputGroup::Genesis, 0, PadButton::B, 0, InputBinding::joyAxis(1, 1));
        config.assign(InputGroup::Genesis, 0, PadButton::Up, 0, InputBinding::joyHat(0, InputBinding::kHatUp));
        manager.setConfig(config);
        QCOMPARE(manager.controllerNames().size(), 1);

        const auto index = [](PadButton button) { return static_cast<std::size_t>(button); };
        auto state = manager.poll(InputGroup::Genesis, true)[0];
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);

        SDL_SetJoystickVirtualButton(device, 2, true);
        SDL_SetJoystickVirtualAxis(device, 1, 30000);
        SDL_SetJoystickVirtualHat(device, 0, SDL_HAT_UP);
        state = manager.poll(InputGroup::Genesis, true)[0];
        QVERIFY(state[index(PadButton::A)]);
        QVERIFY(state[index(PadButton::B)]);
        QVERIFY(state[index(PadButton::Up)]);
        QVERIFY(!state[index(PadButton::C)]);

        state = manager.poll(InputGroup::Genesis, false)[0];
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);

        SDL_SetJoystickVirtualButton(device, 2, false);
        SDL_SetJoystickVirtualAxis(device, 1, 0);
        SDL_SetJoystickVirtualHat(device, 0, SDL_HAT_CENTERED);
        state = manager.poll(InputGroup::Genesis, true)[0];
        QVERIFY(!state[index(PadButton::A)] && !state[index(PadButton::B)] && !state[index(PadButton::Up)]);
    }

    void pressedWithUsesTheGivenBindingsAndIgnoresWindowFocus()
    {
        InputManager manager; // keeps the default bindings: no joystick input is bound
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 1, 4, 0);
        QVERIFY2(device, SDL_GetError());
        QCOMPARE(manager.controllerNames().size(), 1);

        InputConfig edited = InputConfig::defaults();
        edited.assign(InputGroup::Genesis, 0, PadButton::C, 0, InputBinding::joyButton(3));
        const auto index = [](PadButton button) { return static_cast<std::size_t>(button); };

        SDL_SetJoystickVirtualButton(device, 3, true);
        QVERIFY(manager.pressedWith(InputGroup::Genesis, 0, edited)[index(PadButton::C)]);
        QVERIFY(!manager.poll(InputGroup::Genesis, true)[0][index(PadButton::C)]);
        SDL_SetJoystickVirtualButton(device, 3, false);
        QVERIFY(!manager.pressedWith(InputGroup::Genesis, 0, edited)[index(PadButton::C)]);
    }

    void aDisconnectedControllerReleasesItsButtons()
    {
        InputManager manager;
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(1));
        manager.setConfig(config);
        const auto index = [](PadButton button) { return static_cast<std::size_t>(button); };

        {
            VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 1, 2, 0);
            QVERIFY2(device, SDL_GetError());
            QCOMPARE(manager.controllerNames().size(), 1);
            SDL_SetJoystickVirtualButton(device, 1, true);
            QVERIFY(manager.poll(InputGroup::Genesis, true)[0][index(PadButton::A)]);
        } // the device is unplugged while the button is held

        QCOMPARE(manager.controllerNames().size(), 0);
        QVERIFY(!manager.poll(InputGroup::Genesis, true)[0][index(PadButton::A)]);

        // Plugging a controller back in works without restarting the manager.
        VirtualJoystick again(SDL_JOYSTICK_TYPE_UNKNOWN, 1, 2, 0);
        QVERIFY2(again, SDL_GetError());
        QCOMPARE(manager.controllerNames().size(), 1);
        SDL_SetJoystickVirtualButton(again, 1, true);
        QVERIFY(manager.poll(InputGroup::Genesis, true)[0][index(PadButton::A)]);
    }

    void axisBelowTheThresholdIsIgnored()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 1, 0);
        QVERIFY2(device, SDL_GetError());
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::Left, 0, InputBinding::joyAxis(0, -1));
        manager.setConfig(config);
        manager.controllerNames();

        SDL_SetJoystickVirtualAxis(device, 0, -8000);
        QVERIFY(!manager.poll(InputGroup::Genesis, true)[0][static_cast<std::size_t>(PadButton::Left)]);
        SDL_SetJoystickVirtualAxis(device, 0, -30000);
        QVERIFY(manager.poll(InputGroup::Genesis, true)[0][static_cast<std::size_t>(PadButton::Left)]);
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
        const int assigned = manager.autoAssign(InputGroup::Genesis, 0, 0, QString(), config, &message);
        if (assigned == 0)
            QSKIP(qPrintable("SDL gave the virtual device no gamepad mapping: " + message));
        QVERIFY2(assigned >= 8, qPrintable(message));
        QVERIFY(!config.bindings(InputGroup::Genesis, 0, PadButton::B)[0].isNone());
        QVERIFY(!config.bindings(InputGroup::Genesis, 0, PadButton::Left)[2].isNone());
        // The keyboard column is left alone.
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void eachPlayerHasKeyboardDefaultsAndTheyNeverOverlap()
    {
        const InputConfig config = InputConfig::defaults();
        for (const InputGroupDef& group : allInputGroups()) {
            QSet<int> keys;
            for (int player = 0; player < InputConfig::kPlayers; ++player) {
                for (const InputAction& action : group.actions) {
                    if (isTurbo(action.button))
                        continue; // turbo rows start empty
                    const InputBinding& binding = config.bindings(group.id, player, action.button)[1];
                    QVERIFY2(binding.type == InputBinding::Type::Key, qPrintable(group.key + " " + action.label));
                    QVERIFY2(!keys.contains(binding.code), qPrintable(group.key + " " + action.label));
                    keys.insert(binding.code);
                }
            }
        }
    }

    void aKeyCanDriveOnlyOneActionEvenAcrossPlayers()
    {
        InputConfig config = InputConfig::defaults();
        const auto displaced = config.assign(InputGroup::Genesis, 1, PadButton::Up, 0, InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(displaced.has_value());
        QCOMPARE(*displaced, (InputConfig::Displaced{0, PadButton::A}));
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1].isNone());
        QCOMPARE(config.bindings(InputGroup::Genesis, 1, PadButton::Up)[0], InputBinding::key(Qt::Key_Z, 'Z'));
    }

    void controllerInputsAreIndependentPerPlayer()
    {
        InputConfig config = InputConfig::defaults();
        QVERIFY(!config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(3)).has_value());
        QVERIFY(!config.assign(InputGroup::Genesis, 1, PadButton::Start, 0, InputBinding::joyButton(3)).has_value());
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::A)[0], InputBinding::joyButton(3));
        QCOMPARE(config.bindings(InputGroup::Genesis, 1, PadButton::Start)[0], InputBinding::joyButton(3));

        // Inside one player the input still belongs to a single action.
        const auto displaced = config.assign(InputGroup::Genesis, 1, PadButton::B, 0, InputBinding::joyButton(3));
        QCOMPARE(*displaced, (InputConfig::Displaced{1, PadButton::Start}));
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::A)[0], InputBinding::joyButton(3));
    }

    void resetPlayerLeavesTheOtherPlayerAndThePadTypeAlone()
    {
        InputConfig config = InputConfig::defaults();
        config.setPadType(PadType::ThreeButton);
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(1));
        config.assign(InputGroup::Genesis, 1, PadButton::A, 0, InputBinding::joyButton(2));
        config.resetPlayer(InputGroup::Genesis, 1);
        QVERIFY(config.bindings(InputGroup::Genesis, 1, PadButton::A)[0].isNone());
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::A)[0], InputBinding::joyButton(1));
        QCOMPARE(config.padType(), PadType::ThreeButton);
    }

    void playersAndDevicesSurviveTheFile()
    {
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::MasterSystem, 1, PadButton::B, 0, InputBinding::joyButton(5));
        config.setDevice(1, "8BitDo Pro 2");
        const InputConfig loaded = InputConfig::fromJson(config.toJson());
        QCOMPARE(loaded, config);
        QCOMPARE(loaded.device(1), QString("8BitDo Pro 2"));
        QVERIFY(loaded.device(0).isEmpty());
    }

    void aVersionOneFileLoadsAsPlayerOne()
    {
        const QJsonObject v1{{"version", 1},
                             {"genesis_pad", 3},
                             {"groups", QJsonObject{{"genesis", QJsonObject{{"a", QJsonArray{InputBinding::joyButton(9).toJson()}}}}}}};
        QString error;
        const InputConfig loaded = InputConfig::fromJson(v1, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.padType(), PadType::ThreeButton);
        QCOMPARE(loaded.bindings(InputGroup::Genesis, 0, PadButton::A)[0], InputBinding::joyButton(9));
        QCOMPARE(loaded.bindings(InputGroup::Genesis, 1, PadButton::A)[1], InputBinding::key(Qt::Key_7, '7'));
    }

    void eachPlayerReadsTheirOwnController()
    {
        InputManager manager;
        VirtualJoystick first(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 3, 0);
        VirtualJoystick second(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 3, 0);
        QVERIFY2(first && second, SDL_GetError());

        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(0));
        config.assign(InputGroup::Genesis, 1, PadButton::A, 0, InputBinding::joyButton(0));
        manager.setConfig(config);
        QCOMPARE(manager.controllerNames().size(), 2);
        const auto a = static_cast<std::size_t>(PadButton::A);

        SDL_SetJoystickVirtualButton(first, 0, true);
        auto states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(states[0][a] && !states[1][a]);

        SDL_SetJoystickVirtualButton(first, 0, false);
        SDL_SetJoystickVirtualButton(second, 0, true);
        states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!states[0][a] && states[1][a]);
    }

    void aPlayerCanPickTheirControllerByName()
    {
        InputManager manager;
        VirtualJoystick padB(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 2, 0, "Pad B");
        VirtualJoystick padA(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 2, 0, "Pad A");
        QVERIFY2(padA && padB, SDL_GetError());

        // Pad B was plugged in first, yet player 2 asks for it, so player 1 gets the other one.
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(0));
        config.assign(InputGroup::Genesis, 1, PadButton::A, 0, InputBinding::joyButton(0));
        config.setDevice(1, "Pad B");
        manager.setConfig(config);
        QCOMPARE(manager.controllerNames().size(), 2);
        const auto a = static_cast<std::size_t>(PadButton::A);

        SDL_SetJoystickVirtualButton(padA, 0, true);
        auto states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(states[0][a] && !states[1][a]);
        SDL_SetJoystickVirtualButton(padA, 0, false);
        SDL_SetJoystickVirtualButton(padB, 0, true);
        states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!states[0][a] && states[1][a]);

        // A name that matches nothing leaves that player without a controller.
        config.setDevice(1, "Pad C");
        manager.setConfig(config);
        states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!states[1][a]);
        QVERIFY(manager.pressedWith(InputGroup::Genesis, 1, config)[a] == false);
    }

    void aSingleControllerBelongsToPlayerOneOnly()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 2, 0);
        QVERIFY2(device, SDL_GetError());
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(0));
        config.assign(InputGroup::Genesis, 1, PadButton::A, 0, InputBinding::joyButton(0));
        manager.setConfig(config);
        manager.controllerNames();

        SDL_SetJoystickVirtualButton(device, 0, true);
        const auto states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(states[0][static_cast<std::size_t>(PadButton::A)]);
        QVERIFY(!states[1][static_cast<std::size_t>(PadButton::A)]);

        QString message;
        QCOMPARE(manager.autoAssign(InputGroup::Genesis, 1, 0, QString(), config, &message), 0);
        QVERIFY(message.contains("player 2"));
    }

    void turboRowsAreSeparateBindingsThatStartEmpty()
    {
        InputConfig config = InputConfig::defaults();
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::TurboA)[1].isNone());
        QVERIFY(!config.assign(InputGroup::Genesis, 0, PadButton::TurboA, 0, InputBinding::joyButton(5)).has_value());
        QCOMPARE(InputConfig::fromJson(config.toJson()), config);
        QCOMPARE(turboTarget(PadButton::TurboA), PadButton::A);
        QCOMPARE(turboTarget(PadButton::TurboZ), PadButton::Z);
        QVERIFY(isTurbo(PadButton::TurboX) && !isTurbo(PadButton::Mode));
    }

    void portModeAndDefaultKeyboardSlot()
    {
        InputConfig config = InputConfig::defaults();
        QCOMPARE(config.portMode(), InputConfig::PortMode::TwoControllers);
        config.setPortMode(InputConfig::PortMode::OneController);
        QCOMPARE(InputConfig::fromJson(config.toJson()).portMode(), InputConfig::PortMode::OneController);

        config.clearPlayer(InputGroup::Genesis, 0);
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1].isNone());
        config.applyDefaultKeyboard(InputGroup::Genesis, 0, 3);
        QCOMPARE(config.bindings(InputGroup::Genesis, 0, PadButton::A)[3], InputBinding::key(Qt::Key_Z, 'Z'));
        QVERIFY(config.bindings(InputGroup::Genesis, 0, PadButton::A)[1].isNone());
        // The other player was not touched.
        QCOMPARE(config.bindings(InputGroup::Genesis, 1, PadButton::A)[1], InputBinding::key(Qt::Key_7, '7'));
    }

    void swappingExchangesThePlayersAndBackgroundPadsStillCount()
    {
        InputManager manager;
        VirtualJoystick first(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 2, 0);
        VirtualJoystick second(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 2, 0);
        QVERIFY2(first && second, SDL_GetError());
        InputConfig config = InputConfig::defaults();
        config.assign(InputGroup::Genesis, 0, PadButton::A, 0, InputBinding::joyButton(0));
        config.assign(InputGroup::Genesis, 1, PadButton::A, 0, InputBinding::joyButton(0));
        manager.setConfig(config);
        manager.controllerNames();
        const auto a = static_cast<std::size_t>(PadButton::A);

        SDL_SetJoystickVirtualButton(first, 0, true);
        QVERIFY(manager.poll(InputGroup::Genesis, true)[0][a]);
        manager.setSwapped(true);
        QVERIFY(manager.swapped());
        auto states = manager.poll(InputGroup::Genesis, true);
        QVERIFY(!states[0][a] && states[1][a]);
        manager.setSwapped(false);

        // Without the window focus the gamepad only counts when allowed.
        QVERIFY(!manager.poll(InputGroup::Genesis, false)[0][a]);
        QVERIFY(manager.poll(InputGroup::Genesis, false, true)[0][a]);
    }

    void joystickDownAndRawStateSeeEveryController()
    {
        InputManager manager;
        VirtualJoystick device(SDL_JOYSTICK_TYPE_UNKNOWN, 0, 4, 1);
        QVERIFY2(device, SDL_GetError());
        manager.controllerNames();
        SDL_SetJoystickVirtualButton(device, 2, true);
        SDL_SetJoystickVirtualHat(device, 0, SDL_HAT_LEFT);
        QVERIFY(manager.joystickDown(InputBinding::joyButton(2)));
        QVERIFY(!manager.joystickDown(InputBinding::joyButton(1)));
        QVERIFY(manager.joystickDown(InputBinding::joyHat(0, InputBinding::kHatLeft)));
        const QString raw = manager.debugRawState();
        QVERIFY2(raw.contains("btn[2]") && raw.contains("hat[0=LEFT]"), qPrintable(raw));
    }

    void keyBindingsKeepTheirModifiers()
    {
        const InputBinding plain = InputBinding::key(Qt::Key_S, 0);
        const InputBinding ctrl = InputBinding::key(Qt::Key_S, 0, Qt::ControlModifier);
        QVERIFY(!plain.sameSource(ctrl));
        QVERIFY(ctrl.sameSource(InputBinding::key(Qt::Key_S, 0, Qt::ControlModifier)));
        QCOMPARE(InputBinding::fromJson(ctrl.toJson()), ctrl);
        QCOMPARE(InputBinding::fromJson(plain.toJson()).modifiers, 0);
        QVERIFY(ctrl.displayName().contains("Ctrl"));
    }

    void autoAssignWithoutControllersSaysSo()
    {
        InputManager manager;
        InputConfig config = InputConfig::defaults();
        QString message;
        QCOMPARE(manager.autoAssign(InputGroup::Genesis, 0, 0, QString(), config, &message), 0);
        QVERIFY(!message.isEmpty());
        QCOMPARE(config, InputConfig::defaults());
    }
};

QTEST_MAIN(InputTests)
#include "InputTests.moc"
