#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QSaveFile>
#include <QStackedWidget>
#include <QStatusBar>

#include "Achievements.h"
#include "AchievementsToken.h"
#include "AppPaths.h"
#include "OsdOverlay.h"
#include "Icons.h"
#include "LibraryPage.h"

namespace {

constexpr int kVolumeStep = 5;
constexpr int kMaxRecent = 10;
constexpr int kJoystickPollMs = 8;
constexpr int kOverlayPollMs = 200;

bool isDown(const QKeyEvent* event) { return event->type() == QEvent::KeyPress; }

}

MainWindow::MainWindow(LibretroCore& core, QWidget* parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_video(new VideoWidget(this))
    , m_library(new LibraryPage(this))
    , m_central(new QStackedWidget(this))
{
    setWindowTitle("Xfusion");
    m_achievements = new Achievements(this);
    connect(m_achievements, &Achievements::message, this, [this](const QString& text, const QImage& icon) { m_video->osd()->post(text, m_settings.achievements.durationSeconds * 1000, icon); });
    // A new session token is kept, so the next start signs in by itself.
    connect(m_achievements, &Achievements::accountChanged, this, [this] {
        const Achievements::User user = m_achievements->user();
        if (!user.loggedIn)
            return;
        const QString token = encryptToken(user.username, user.token);
        if (m_settings.achievements.username == user.username && decryptToken(user.username, m_settings.achievements.token) == user.token)
            return;
        m_settings.achievements.username = user.username;
        m_settings.achievements.token = token;
        QString error;
        if (!m_settings.save(AppPaths::settingsPath(), &error))
            statusBar()->showMessage(error);
    });
    // The library is what is shown while no game is open; the picture takes over when one starts.
    m_central->addWidget(m_library);
    m_central->addWidget(m_video);
    setCentralWidget(m_central);
    connect(m_library, &LibraryPage::gameActivated, this, [this](const QString& path) { openGame(path); });
    connect(m_library, &LibraryPage::message, this, [this](const QString& text) { statusBar()->showMessage(text, 6000); });
    connect(m_library, &LibraryPage::folderChosen, this, [this](const QString& folder) {
        if (!m_settings.libraryFolders.contains(folder))
            m_settings.libraryFolders << folder;
        commitSettings("Library folder added");
    });
    setFocusPolicy(Qt::StrongFocus);
    resize(960, 720);

    m_core.setVideoCallback([this](const QImage& frame, double aspect) { m_video->submitFrame(frame, aspect); });
    m_core.setAudioCallback([this](const int16_t* samples, size_t frames) {
        if (!m_audio)
            return;
        if (m_emu) {
            if (m_emu->rewinding())
                return; // rewinding is always silent
            if (m_emu->stepping() && m_muteStepping.load(std::memory_order_relaxed))
                return;
            if (m_emu->fastForward() && m_muteFastForward.load(std::memory_order_relaxed))
                return;
        }
        m_audio->push(samples, frames);
    });

    AudioOutput::playbackDevices(); // listed now, while no game is playing
    createMenus();

    QString configError;
    m_input.setConfig(InputConfig::load(AppPaths::inputConfigPath(), &configError));
    QString settingsError;
    m_settings = AppSettings::load(AppPaths::settingsPath(), &settingsError);
    m_backgroundPads = m_settings.backgroundGamepad;
    loadSession();
    m_video->setDriver(m_settings.videoDriver);
    applySettings();
    if (configError.isEmpty())
        configError = settingsError;
    if (!configError.isEmpty()) {
        statusBar()->showMessage(configError);
        m_video->osd()->post(configError);
    }

    m_saveTimer.setSingleShot(false);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { flushSaveRam(false); });
    m_overlayTimer.setInterval(kOverlayPollMs);
    connect(&m_overlayTimer, &QTimer::timeout, this, &MainWindow::updateOverlays);
    m_overlayTimer.start();
    m_playTimer.setInterval(60000);
    connect(&m_playTimer, &QTimer::timeout, this, &MainWindow::commitPlayTime);
    m_playTimer.start();
    m_joystickTimer.setInterval(kJoystickPollMs);
    connect(&m_joystickTimer, &QTimer::timeout, this, &MainWindow::pollShortcutJoysticks);
    m_joystickTimer.start();

    // Shortcuts are matched by key and exact modifiers here, not by QAction, so that hold actions, four bindings
    // per action and gamepad inputs all work the same way.
    qApp->installEventFilter(this);
    updateActions();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

// ---------------------------------------------------------------------------------------------------- menus

void MainWindow::createMenus()
{
    createFileMenu();
    createEmulationMenu();
    createViewMenu();
    createOptionsMenu();
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About...", this, [this] {
        QMessageBox::about(this, "About Xfusion",
                           QString("<b>Xfusion %1</b> <s>sex edition</s><br>A frontend for SEGA systems up to the fourth generation, running libretro cores. "
                                   "Twin of snes9xrd.<br><br>Cores it runs:<br>"
                                   "Genesis Plus GX (c) Charles MacDonald, Eke-Eke - SG-1000, Master System, Game Gear, Genesis, Sega CD<br>"
                                   "PicoDrive by notaz and contributors - 32X<br><br>Core in use: %2<br><br>"
                                   "The cores are separate programs with their own licenses; neither may be sold or used commercially. "
                                   "SEGA is a trademark.").arg(QCoreApplication::applicationVersion(), m_core.name().isEmpty() ? QString("none installed (Options > Emulation > Manage Cores)") : m_core.name()));
    });
}

void MainWindow::createFileMenu()
{
    QMenu* menu = menuBar()->addMenu("&File");
    const auto add = [this, menu](ShortcutAction id, const QString& text, auto&& handler) {
        QAction* action = menu->addAction(text, this, handler);
        m_actions[id] = action;
        m_actionTexts[id] = text;
        return action;
    };
    setThemedActionIcon(add(ShortcutAction::OpenRom, "&Open File...", [this] { promptOpenGame(); }), "open");
    m_recentMenu = menu->addMenu("Recent Files");
    connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::populateRecentMenu);
    menu->addSeparator();

    m_saveMenu = menu->addMenu("Save Game Position");
    populatePositionMenu(m_saveMenu, true);
    m_loadMenu = menu->addMenu("Load Game Position");
    populatePositionMenu(m_loadMenu, false);
    QAction* savePreview = menu->addAction("Save with Preview", this, [this] { showStatePreview(true); });
    QAction* loadPreview = menu->addAction("Load with Preview", this, [this] { showStatePreview(false); });
    menu->addSeparator();

    QMenu* other = menu->addMenu("Save &Other");
    QAction* screenshot = other->addAction("Save Screenshot", this, [this] { takeScreenshot(); });
    m_actions[ShortcutAction::Screenshot] = screenshot;
    m_actionTexts[ShortcutAction::Screenshot] = "Save Screenshot";
    QAction* sram = other->addAction("Save S-RAM Data", this, [this] { saveSramNow(); });
    m_romInfoAction = menu->addAction("ROM &Information...", this, [this] { showRomInfo(); });
    menu->addSeparator();

    QAction* reset = add(ShortcutAction::Reset, "&Reset Game", [this] { resetGame(false); });
    setThemedActionIcon(reset, "refresh");
    m_closeAction = menu->addAction("&Close Game", this, [this] { closeGame(); });
    setThemedActionIcon(m_closeAction, "exit");
    setThemedActionIcon(add(ShortcutAction::Quit, "E&xit", [this] { close(); }), "exit");

    m_gameActions = {screenshot, sram, m_romInfoAction, reset, m_closeAction};
    m_stateActions.push_back(savePreview);
    m_stateActions.push_back(loadPreview);
}

void MainWindow::populatePositionMenu(QMenu* menu, bool saving)
{
    for (int bank = 0; bank < 10; ++bank) {
        QMenu* bankMenu = menu->addMenu(QString("Bank #%1").arg(bank));
        for (int slot = 0; slot < 10; ++slot) {
            const int position = bank * 10 + slot;
            QAction* action = bankMenu->addAction(QString("Slot #%1").arg(slot), this, [this, position, saving] {
                if (saving)
                    saveStateToSlot(position);
                else
                    loadStateFromSlot(position);
            });
            m_stateActions.push_back(action);
        }
    }
    menu->addSeparator();
    if (!saving) {
        m_stateActions.push_back(menu->addAction("Oops File", this, [this] { loadOopsState(); }));
        menu->addSeparator();
    }
    QAction* select = menu->addAction("Select File", this, [this, saving] {
        if (!stateOperationsAvailable())
            return;
        ++m_dialogPause;
        updatePause();
        const QString filter = "Save States (*.state);;All Files (*)";
        const QString folder = AppPaths::statesDir(*m_gameSystem, m_romDir);
        const QString path = saving ? QFileDialog::getSaveFileName(this, "Choose a State File", folder, filter)
                                    : QFileDialog::getOpenFileName(this, "Choose a State File", folder, filter);
        --m_dialogPause;
        updatePause();
        if (path.isEmpty())
            return;
        if (saving)
            saveStateToFile(path);
        else
            loadStateFromFile(path);
    });
    m_stateActions.push_back(select);
}

void MainWindow::createEmulationMenu()
{
    QMenu* menu = menuBar()->addMenu("&Emulation");
    m_pauseAction = menu->addAction("&Pause", this, [this] { setManualPause(!m_manualPause); });
    setThemedActionIcon(m_pauseAction, "pause");
    m_actions[ShortcutAction::PauseContinue] = m_pauseAction;
    m_actionTexts[ShortcutAction::PauseContinue] = "&Pause";
    m_pauseUnfocusedAction = menu->addAction("Pause &When Inactive");
    m_pauseUnfocusedAction->setCheckable(true);
    connect(m_pauseUnfocusedAction, &QAction::triggered, this, [this](bool on) {
        m_settings.pauseWhenUnfocused = on;
        commitSettings(on ? "Pause when inactive: on" : "Pause when inactive: off");
    });
    QAction* frameAdvance = menu->addAction("&Frame Advance", this, [this] { trigger(ShortcutAction::FrameAdvance); });
    m_actions[ShortcutAction::FrameAdvance] = frameAdvance;
    m_actionTexts[ShortcutAction::FrameAdvance] = "&Frame Advance";
    menu->addSeparator();
    QAction* reset = menu->addAction("Rese&t", this, [this] { resetGame(false); });
    setThemedActionIcon(reset, "refresh");
    QAction* hardReset = menu->addAction("&Hard Reset", this, [this] { resetGame(true); });
    setThemedActionIcon(hardReset, "reset");
    m_actions[ShortcutAction::PowerCycle] = hardReset;
    m_actionTexts[ShortcutAction::PowerCycle] = "&Hard Reset";
    // Only shown for a game with more than one disc.
    m_discMenu = menu->addMenu("&Disc");
    m_discMenu->menuAction()->setVisible(false);
    connect(m_discMenu, &QMenu::aboutToShow, this, &MainWindow::populateDiscMenu);
    m_gameActions.push_back(m_pauseAction);
    m_gameActions.push_back(frameAdvance);
    m_gameActions.push_back(reset);
    m_gameActions.push_back(hardReset);
}

void MainWindow::createViewMenu()
{
    QMenu* menu = menuBar()->addMenu("&View");
    QMenu* size = menu->addMenu("&Set Size");
    for (int multiple = 1; multiple <= 10; ++multiple)
        size->addAction(multiple == 10 ? QString("1&0x") : QString("&%1x").arg(multiple), this, [this, multiple] { resizeToMultiple(multiple); });
    menu->addSeparator();
    QAction* refresh = menu->addAction("&Refresh Library\tF5", this, [this] { m_library->refresh(); });
    refresh->setToolTip("Scan the library folders again (F5 while the library is shown)");
    menu->addSeparator();
    QAction* fullscreen = menu->addAction("&Fullscreen", this, [this] { toggleFullscreen(); });
    setThemedActionIcon(fullscreen, "fullscreen");
    m_actions[ShortcutAction::ToggleFullscreen] = fullscreen;
    m_actionTexts[ShortcutAction::ToggleFullscreen] = "&Fullscreen";
}

void MainWindow::createOptionsMenu()
{
    QMenu* menu = menuBar()->addMenu("&Options");
    const char* const labels[] = {"&General...", "&Display...", "&Sound...", "&Emulation...", "&Controllers...", "Shortcu&ts...", "&Files...", "&Achievements..."};
    const char* const icons[] = {"settings", "display", "sound", "emulation", "joypad", "keyboard", "folders", "achievements"};
    for (int page = 0; page < 8; ++page)
        setThemedActionIcon(menu->addAction(labels[page], this, [this, page] { openSettings(page); }), icons[page]);
    menu->addSeparator();
    m_shaderParametersAction = menu->addAction("Shader &Parameters...", this, [this] { openShaderParameters(); });
    m_shaderParametersAction->setEnabled(false);
}

void MainWindow::refreshMenuHints()
{
    for (const auto& [id, action] : m_actions) {
        QString hint;
        for (const InputBinding& binding : m_settings.shortcuts.bindings(id)) {
            if (!binding.isNone()) {
                hint = binding.displayName();
                break;
            }
        }
        const QString base = m_actionTexts[id];
        action->setText(hint.isEmpty() ? base : base + "\t" + hint);
    }
}

void MainWindow::populateRecentMenu()
{
    m_recentMenu->clear();
    if (m_recent.isEmpty()) {
        m_recentMenu->addAction("No recent files")->setEnabled(false);
        return;
    }
    for (int i = 0; i < m_recent.size(); ++i) {
        const QString path = m_recent[i];
        m_recentMenu->addAction(QString("&%1: %2").arg(i).arg(QDir::toNativeSeparators(path)), this, [this, path] { openGame(path); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Clear Recent Files", this, [this] {
        m_recent.clear();
        saveSession();
    });
}

// ----------------------------------------------------------------------------------------------- shortcuts

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
        return QMainWindow::eventFilter(watched, event);

    // Only the main window reacts: dialogs (the settings capture, file choosers) keep their own keys.
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget || widget->window() != this || QApplication::activeWindow() != this)
        return false;
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_unknown)
        return false;
    // Typing in the library search is typing, not shortcuts.
    if (qobject_cast<QLineEdit*>(QApplication::focusWidget()))
        return false;
    // While the library is what is shown, plain keys belong to it: letters search the list (P and Y are shortcuts
    // of the game, and would never reach it), and F5 scans the folders again instead of loading a state.
    if (m_central->currentWidget() == m_library) {
        const bool chorded = keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        const bool functionKey = keyEvent->key() >= Qt::Key_F1 && keyEvent->key() <= Qt::Key_F35;
        if (keyEvent->key() == Qt::Key_F5 && !chorded && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            if (isDown(keyEvent) && !keyEvent->isAutoRepeat())
                m_library->refresh();
            return true;
        }
        if (!chorded && !functionKey)
            return false;
    }
    const bool down = isDown(keyEvent);

    // Keys that are not Windows-polled (other platforms) still reach the pad through here.
    if (!keyEvent->isAutoRepeat())
        m_input.setKeyDown(keyEvent->key(), down);

    // Releasing a key ends the hold actions it started, whatever modifiers are down by then.
    // While a key is held the system repeats it, and Qt reports each repeat as a release followed by a press, both
    // flagged auto-repeat. Those are not the key going up: only the real release ends a hold, or fast-forward would stop
    // as soon as the repeat starts.
    if (!down) {
        const auto held = m_heldByKey.find(keyEvent->key());
        if (keyEvent->isAutoRepeat())
            return held != m_heldByKey.end();
        if (held != m_heldByKey.end()) {
            for (ShortcutAction action : held->second)
                setHold(action, false);
            m_heldByKey.erase(held);
            return true;
        }
        return false;
    }
    if (keyEvent->isAutoRepeat())
        return (keyEvent->key() == Qt::Key_Escape && isFullScreen()) || m_heldByKey.contains(keyEvent->key());

    // Esc leaves fullscreen and cannot be rebound.
    if (keyEvent->key() == Qt::Key_Escape && isFullScreen()) {
        toggleFullscreen();
        return true;
    }
    const std::vector<ShortcutAction> matches = m_settings.shortcuts.matchKey(keyEvent->key(), static_cast<int>(keyEvent->modifiers()));
    for (ShortcutAction action : matches) {
        if (shortcutDef(action).hold) {
            m_heldByKey[keyEvent->key()].push_back(action);
            setHold(action, true);
        } else {
            trigger(action);
        }
    }
    return !matches.empty();
}

void MainWindow::rebuildJoystickShortcuts()
{
    m_joystickActions.clear();
    for (const ShortcutDef& def : allShortcuts()) {
        for (const InputBinding& binding : m_settings.shortcuts.bindings(def.id)) {
            if (!binding.isNone() && binding.type != InputBinding::Type::Key) {
                m_joystickActions.push_back(def.id);
                break;
            }
        }
    }
}

void MainWindow::pollShortcutJoysticks()
{
    if (m_joystickActions.empty())
        return;
    const bool allowed = (isActiveWindow() && QApplication::activeModalWidget() == nullptr) || (m_settings.backgroundGamepad && QApplication::activeModalWidget() == nullptr);
    for (ShortcutAction action : m_joystickActions) {
        bool down = false;
        if (allowed) {
            for (const InputBinding& binding : m_settings.shortcuts.bindings(action)) {
                if (!binding.isNone() && binding.type != InputBinding::Type::Key && m_input.joystickDown(binding)) {
                    down = true;
                    break;
                }
            }
        }
        bool& was = m_joyDown[static_cast<std::size_t>(action)];
        if (down && !was) {
            if (shortcutDef(action).hold)
                setHold(action, true);
            else
                trigger(action);
        } else if (!down && was && shortcutDef(action).hold) {
            setHold(action, false);
        }
        was = down;
    }
}

void MainWindow::setHold(ShortcutAction action, bool down)
{
    if (action == ShortcutAction::FastForward) {
        m_fastForwardHeld = down;
        updateAudioMode();
    } else if (action == ShortcutAction::Rewind) {
        if (!m_emu)
            return;
        if (down && !m_emu->rewindAvailable()) {
            showMessage("Rewind is off: set a rewind buffer in Options > Emulation");
            return;
        }
        m_emu->setRewinding(down);
        updateAudioMode();
    }
}

void MainWindow::trigger(ShortcutAction action)
{
    const int index = static_cast<int>(action);
    if (index >= static_cast<int>(ShortcutAction::QuickSave0) && index < static_cast<int>(ShortcutAction::QuickSave0) + kQuickSlots) {
        saveStateToSlot(index - static_cast<int>(ShortcutAction::QuickSave0));
        return;
    }
    if (index >= static_cast<int>(ShortcutAction::QuickLoad0) && index < static_cast<int>(ShortcutAction::QuickLoad0) + kQuickSlots) {
        loadStateFromSlot(index - static_cast<int>(ShortcutAction::QuickLoad0));
        return;
    }
    switch (action) {
    case ShortcutAction::OpenRom: promptOpenGame(); break;
    case ShortcutAction::ToggleFastForward:
        m_fastForwardToggled = !m_fastForwardToggled;
        updateAudioMode();
        showMessage(m_fastForwardToggled ? "Turbo mode: on" : "Turbo mode: off");
        break;
    case ShortcutAction::PauseContinue:
        if (m_emu)
            setManualPause(!m_manualPause);
        break;
    case ShortcutAction::Reset:
        if (m_emu)
            resetGame(false);
        break;
    case ShortcutAction::PowerCycle:
        if (m_emu)
            resetGame(true);
        break;
    case ShortcutAction::NextDisc:
        if (m_emu)
            switchDisc(1);
        break;
    case ShortcutAction::PreviousDisc:
        if (m_emu)
            switchDisc(-1);
        break;
    case ShortcutAction::Quit: close(); break;
    case ShortcutAction::ToggleFullscreen: toggleFullscreen(); break;
    case ShortcutAction::Screenshot: takeScreenshot(); break;
    case ShortcutAction::SaveState: saveStateToSlot(m_currentSlot, true); break;
    case ShortcutAction::LoadState: loadStateFromSlot(m_currentSlot, true); break;
    case ShortcutAction::UndoState: loadUndoState(); break;
    case ShortcutAction::IncreaseSlot: setCurrentSlot(m_currentSlot + 1); break;
    case ShortcutAction::DecreaseSlot: setCurrentSlot(m_currentSlot - 1); break;
    case ShortcutAction::SwapControllers:
        if (m_emu) {
            m_input.setSwapped(!m_input.swapped());
            showMessage(m_input.swapped() ? "Swap pads: P1=Joypad2 P2=Joypad1" : "Swap pads: P1=Joypad1 P2=Joypad2");
        }
        break;
    case ShortcutAction::SpeedUp: changeSpeed(1); break;
    case ShortcutAction::SpeedDown: changeSpeed(-1); break;
    case ShortcutAction::FrameSkipUp: changeFrameSkip(1); break;
    case ShortcutAction::FrameSkipDown: changeFrameSkip(-1); break;
    case ShortcutAction::FrameAdvance:
        if (m_emu) {
            setManualPause(true);
            m_emu->stepFrame();
        }
        break;
    case ShortcutAction::Mute: toggleMute(); break;
    case ShortcutAction::VolumeDown: adjustVolume(-kVolumeStep); break;
    case ShortcutAction::VolumeUp: adjustVolume(kVolumeStep); break;
    default: break;
    }
}

// -------------------------------------------------------------------------------------------------- session

void MainWindow::loadSession()
{
    QFile file(AppPaths::configDir() + "/session.json");
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue value : object.value("recent").toArray())
        if (m_recent.size() < kMaxRecent)
            m_recent << value.toString();
    m_lastDir = object.value("last_dir").toString();
    const int width = object.value("width").toInt();
    const int height = object.value("height").toInt();
    if (width >= 320 && height >= 240)
        resize(width, height);
}

void MainWindow::saveSession() const
{
    QJsonObject object{{"version", 1}, {"recent", QJsonArray::fromStringList(m_recent)}, {"last_dir", m_lastDir}};
    if (!isFullScreen() && !isMaximized()) {
        object.insert("width", width());
        object.insert("height", height());
    }
    QSaveFile file(AppPaths::configDir() + "/session.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void MainWindow::addRecent(const QString& path)
{
    m_recent.removeAll(path);
    m_recent.prepend(path);
    while (m_recent.size() > kMaxRecent)
        m_recent.removeLast();
    saveSession();
}

// ------------------------------------------------------------------------------------------------- window

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange) {
        m_windowActive = isActiveWindow();
        m_backgroundPads = m_settings.backgroundGamepad;
        if (!m_windowActive) {
            m_input.clearKeys();
            // Not when one of our own dialogs took the focus: only when the whole application lost it.
            m_focusPause = m_settings.pauseWhenUnfocused && QGuiApplication::applicationState() != Qt::ApplicationActive;
        } else {
            m_focusPause = false;
        }
        updatePause();
    } else if (event->type() == QEvent::WindowStateChange) {
        m_minimizedPause = isMinimized();
        updatePause();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    closeGame();
    saveSession();
    event->accept();
}
