#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStackedWidget>
#include <QStatusBar>

#include <algorithm>
#include <cstdio>

#include "AppPaths.h"
#include "Bios.h"
#include "CoreRules.h"
#include "DiscInfo.h"
#include "GameHeader.h"
#include "Patch.h"
#include "RomInfo.h"
#include "OsdOverlay.h"
#include "SettingsDialog.h"
#include "StatePreviewDialog.h"
#include "StateFile.h"
#include "SystemInfo.h"
#include "Icons.h"
#include "LibraryPage.h"
#include "Theme.h"

namespace {

constexpr int kVolumeStep = 5;
constexpr int kMaxRecent = 10;
constexpr qint64 kOopsAfterMs = 300'000; // an automatic snapshot before a reset when nothing was saved for this long
constexpr int kJoystickPollMs = 8;
constexpr int kOverlayPollMs = 200;
constexpr int kThumbnailWidth = 160;

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
                                   "SEGA is a trademark.").arg(QCoreApplication::applicationVersion().isEmpty() ? "0.1" : QCoreApplication::applicationVersion(), m_core.name()));
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
    const char* const labels[] = {"&General...", "&Display...", "&Sound...", "&Emulation...", "&Controllers...", "Shortcu&ts...", "&Files..."};
    const char* const icons[] = {"settings", "display", "sound", "emulation", "joypad", "keyboard", "folders"};
    for (int page = 0; page < 7; ++page)
        setThemedActionIcon(menu->addAction(labels[page], this, [this, page] { openSettings(page); }), icons[page]);
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
        return (keyEvent->key() == Qt::Key_Escape && isFullScreen()) || m_heldByKey.count(keyEvent->key());

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

// ------------------------------------------------------------------------------------------------ games

void MainWindow::promptOpenGame()
{
    ++m_dialogPause;
    updatePause();
    const QString path = QFileDialog::getOpenFileName(this, "Open a game", m_lastDir, romFileDialogFilter());
    --m_dialogPause;
    updatePause();
    if (!path.isEmpty())
        openGame(path);
}

void MainWindow::openGame(const QString& path)
{
    const QString libraryId = GameSaves::fingerprint(path);
    std::optional<System> system = systemForFile(path);
    if (const auto chosen = m_library->systemChoice(libraryId))
        system = chosen; // the user said what it is
    if (!system) {
        QString why = QString("Unrecognized game format: %1").arg(QDir::toNativeSeparators(path));
        if (QFileInfo(path).suffix().compare("cue", Qt::CaseInsensitive) == 0) {
            const DiscInfo disc = readDisc(path);
            if (!disc.problems.isEmpty())
                why += "\n\n" + disc.problems.join("\n");
        }
        QMessageBox::warning(this, "Xfusion", why);
        return;
    }
    const bool isDisc = *system == System::SegaCD;
    const DiscInfo disc = isDisc ? readDisc(path) : DiscInfo();
    if (isDisc && !prepareDisc(disc))
        return;

    closeGame();
    if (!selectCore(*system, m_library->coreChoice(libraryId)))
        return;
    if (isDisc)
        warnAboutAudio(disc);
    m_lastDir = QFileInfo(path).absolutePath();
    m_romDir = m_lastDir;
    const QString playPath = preparePatchedGame(path, *system);
    if (playPath.isEmpty())
        return;
    m_libraryId = libraryId;

    const QString saveDir = AppPaths::savesDir(*system, m_romDir);
    m_core.setDirectories(AppPaths::systemDir(), saveDir);
    m_saves = std::make_unique<GameSaves>(saveDir, AppPaths::statesDir(*system, m_romDir), playPath);
    if (!m_saves->warning().isEmpty())
        QMessageBox::warning(this, "Xfusion", m_saves->warning() + "\n\nA new index will be started; the old one is kept as saves.json.bad.");

    QString error;
    const InputConfig config = m_input.config();
    std::map<std::string, std::string> coreOptions;
    const auto userOptions = m_settings.coreOptions.value(m_core.profile().id);
    for (auto it = userOptions.begin(); it != userOptions.end(); ++it)
        coreOptions[it.key().toStdString()] = it.value().toStdString();
    m_core.setUserOptions(coreOptions);
    m_core.setPads(config.padType(), config.portMode() == InputConfig::PortMode::TwoControllers,
                   systemDef(*system).inputGroup == InputGroup::Genesis);
    if (!m_core.loadGame(playPath, &error)) {
        if (*system == System::SegaCD) {
            error += QString("\n\nSega CD games need the BIOS for the disc's region in:\n%1\nExpected file names: %2")
                         .arg(AppPaths::systemDir(), m_core.profile().segaCdBiosFiles.join(", "));
        }
        QMessageBox::critical(this, "Xfusion", error);
        m_saves.reset();
        return;
    }

    m_gamePath = path;
    m_gameSystem = system;
    m_lastSaveRam.clear();
    m_saveRamLocked = false;
    m_saveWriteFailing = false;
    m_statesSupported = m_core.supportsStates();
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    m_manualPause = false;
    m_fastForwardHeld = false;
    m_fastForwardToggled = false;
    loadSaveRam();
    const qsizetype sramSize = m_core.saveRam().size();
    m_discSet.clear();
    if (isDisc)
        setupDiscs(path);

    m_audio = std::make_unique<AudioOutput>(m_core.avInfo().sampleRate, m_settings.audioDevice, m_settings.playbackRate);
    m_inputGroup = systemDef(*system).inputGroup;
    m_emu = std::make_unique<EmuThread>(m_core, m_audio.get(), [this] { pollInput(); });
    m_emu->setRewind(static_cast<std::size_t>(m_settings.rewindMegabytes) * 1024 * 1024, m_settings.rewindInterval);
    updateSpeed();
    updateAudioMode();
    m_emu->start();
    if (m_settings.sramInterval > 0)
        m_saveTimer.start(m_settings.sramInterval * 1000);

    m_central->setCurrentWidget(m_video);
    addRecent(path);
    setWindowTitle(QString("Xfusion - %1").arg(QFileInfo(path).completeBaseName()));
    updateActions();
    updatePause();
    setFocus();

    if (m_settings.showRomInfo)
        showMessage(startText(path, playPath, *system, disc, sramSize));
    if (!m_coreNote.isEmpty())
        showMessage(m_coreNote);
    if (m_settings.fullscreenOnOpen && !isFullScreen())
        toggleFullscreen();
    if (m_settings.saveStateOnClose && m_statesSupported && QFile::exists(m_saves->resumeStatePath()))
        loadStateFromFile(m_saves->resumeStatePath());
}

// The message snes9xrd shows for a ROM, for this game (RomInfo.h).
QString MainWindow::startText(const QString& path, const QString& playPath, System system, const DiscInfo& disc, qsizetype sramSize) const
{
    StartInfo info;
    info.system = systemDef(system).folder;
    info.core = coreInfo(m_core.profile().id) ? coreInfo(m_core.profile().id)->name : m_core.name();
    info.pal = m_core.avInfo().fps < 55;
    info.sram = sramSize <= 0 ? QString("none") : sramSize >= 1024 ? QString("%1KB").arg(sramSize / 1024) : QString("%1B").arg(sramSize);
    info.patch = m_patchKind;
    GameHeader header;
    if (system == System::SegaCD) {
        header = disc.header;
        info.size = QString("%1 data / %2 audio %3").arg(disc.dataTracks).arg(disc.audioTracks).arg(disc.audioTracks == 1 ? "track" : "tracks");
    } else {
        QByteArray data;
        QString extension, error;
        if (readGameBytes(playPath, &data, &extension, &error)) {
            header = parseCartridgeHeader(data.left(0x8000), extension);
            info.size = sizeInBits(data.size());
            info.hasCrc = true;
            info.crc = crc32Of(data);
            info.checksum = cartridgeChecksum(data);
        }
    }
    info.title = header.title().isEmpty() ? QFileInfo(path).completeBaseName() : header.title();
    info.product = header.product;
    info.revision = revisionOfProduct(header.product);
    return startMessage(info);
}

// Looks at a Sega CD disc before the core is asked to load it, so that what is wrong is said in plain words: tracks
// missing from the cue sheet, or no BIOS for the region of the disc.
bool MainWindow::prepareDisc(const DiscInfo& disc)
{
    if (!disc.problems.isEmpty())
        QMessageBox::warning(this, "Xfusion", "Some files of this disc are missing, so it may lack its music:\n\n" + disc.problems.join("\n"));

    const unsigned regions = disc.header.regions;
    if (regions == 0)
        return true; // a .chd, or a header that does not say: the core decides
    const auto offered = [&](const BiosStatus& status) {
        return ((regions & GameHeader::Japan) && status.japan) || ((regions & GameHeader::Americas) && status.usa) || ((regions & GameHeader::Europe) && status.europe);
    };
    BiosStatus status = checkBios(AppPaths::systemDir());
    if (offered(status))
        return true;

    QStringList names;
    if (regions & GameHeader::Japan)
        names << biosFileName(BiosRegion::Japan);
    if (regions & GameHeader::Americas)
        names << biosFileName(BiosRegion::USA);
    if (regions & GameHeader::Europe)
        names << biosFileName(BiosRegion::Europe);
    if (names.isEmpty())
        return true; // an Asia-only disc: no BIOS of its own to ask for
    QMessageBox box(QMessageBox::Warning, "Xfusion",
                    QString("This disc is for %1 and needs the Sega CD BIOS for that region: %2.\n\nThey go in %3.\n\n"
                            "If you have BIOS dumps (with any file name), Xfusion can find them and copy them there.")
                        .arg(disc.header.regionText(), names.join(" or "), QDir::toNativeSeparators(AppPaths::systemDir())),
                    QMessageBox::NoButton, this);
    QPushButton* find = box.addButton("Find BIOS Files...", QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != find)
        return false;
    const QString folder = QFileDialog::getExistingDirectory(this, "Folder with the BIOS dumps", m_lastDir);
    if (folder.isEmpty())
        return false;
    QString error;
    const QStringList installed = installBios(folder, AppPaths::systemDir(), &error);
    if (!error.isEmpty())
        QMessageBox::warning(this, "Xfusion", error);
    status = checkBios(AppPaths::systemDir());
    if (!installed.isEmpty())
        showMessage("BIOS installed: " + installed.join(", "));
    if (!offered(status)) {
        QMessageBox::warning(this, "Xfusion", "No BIOS for this disc was found in that folder.");
        return false;
    }
    return true;
}

// Neither core plays every .mp3: Genesis Plus GX none (WAV and OGG only), PicoDrive only MPEG-1 at 44.1 kHz, and
// it skips the others, which shifts the numbers of the tracks after them. Said when the disc is opened.
void MainWindow::warnAboutAudio(const DiscInfo& disc)
{
    int mp3 = 0;
    for (const QString& file : disc.audioFiles)
        if (file.endsWith(".mp3", Qt::CaseInsensitive))
            ++mp3;
    if (mp3 == 0)
        return;
    if (m_core.profile().id == CoreId::GenesisPlusGx) {
        const QString note = QString("%1 of %2 audio tracks are .mp3, which Genesis Plus GX cannot play (WAV or OGG only)").arg(mp3).arg(disc.audioFiles.size());
        m_coreNote = m_coreNote.isEmpty() ? note : m_coreNote + "; " + note;
    } else if (!disc.unplayableMp3.isEmpty()) {
        QMessageBox::warning(this, "Xfusion",
                             QString("%1 of the %2 audio tracks of this disc are .mp3 files in a format PicoDrive cannot play (it plays MPEG-1 at "
                                     "44.1 kHz only; for example %3 is not).\n\nThe music will be missing, and the tracks after each unplayable one "
                                     "will be shifted, so the game may play the wrong music.\n\nConvert the tracks, for instance with ffmpeg: "
                                     "ffmpeg -i \"in.mp3\" -ar 44100 \"out.mp3\".")
                                 .arg(disc.unplayableMp3.size()).arg(disc.audioFiles.size()).arg(disc.unplayableMp3.first()));
    }
}

// Chooses the core for the game (CoreRules) and loads it when it is not the one already loaded.
bool MainWindow::selectCore(System system, const QString& gameCoreChoice)
{
    m_coreNote.clear();
    if (m_coreForced)
        return true;
    const CoreDecision decision = chooseCore(system, m_settings.coreChoices, [](const QString& id) { return AppPaths::coreInstalled(id); }, gameCoreChoice);
    if (decision.id.isEmpty()) {
        QMessageBox::critical(this, "Xfusion",
                              QString("No installed core can play %1 games. Cores go in %2.")
                                  .arg(systemDef(system).folder, QDir::toNativeSeparators(AppPaths::coresDir())));
        return false;
    }
    QString error;
    if (!m_core.useCore(coreProfileById(decision.id), AppPaths::corePath(decision.id), &error)) {
        QMessageBox::critical(this, "Xfusion", error);
        return false;
    }
    m_coreNote = decision.note;
    return true;
}

QString MainWindow::preparePatchedGame(const QString& path, System system)
{
    m_patchedGame.reset();
    m_patchName.clear();
    m_patchKind.clear();
    const QString patchFile = findPatch(path, AppPaths::patchFolders(system, QFileInfo(path).absolutePath()));
    if (patchFile.isEmpty())
        return path;
    if (system == System::SegaCD) {
        // A disc is many files, or a compressed one: there is no single file to patch. Say so instead of ignoring it.
        const bool chd = QFileInfo(path).suffix().compare("chd", Qt::CaseInsensitive) == 0;
        const auto answer = QMessageBox::warning(
            this, "Xfusion",
            QString("There is a patch for this game (%1), but patches cannot be applied to Sega CD discs%2.\n\nOpen the game without it?")
                .arg(QFileInfo(patchFile).fileName(),
                     chd ? " (a .chd is compressed, so its bytes are not where the patch expects them)" : QString()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        return answer == QMessageBox::Yes ? path : QString();
    }

    const QString patchName = QFileInfo(patchFile).fileName();
    QByteArray game, patch;
    QString extension, error;
    PatchResult result;
    QFile file(patchFile);
    if (!readGameBytes(path, &game, &extension, &error)) {
        // Nothing to patch; the game will report its own problem when it loads.
        return path;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        error = QString("%1 could not be read").arg(patchName);
    } else {
        patch = file.readAll();
        result = applyPatch(game, patch);
        if (result.ok)
            error.clear();
        else
            error = result.error;
    }
    if (!error.isEmpty()) {
        const auto answer = QMessageBox::warning(this, "Xfusion",
                                                 QString("The patch %1 could not be applied to %2:\n\n%3\n\nOpen the game without the patch?")
                                                     .arg(patchName, QFileInfo(path).fileName(), error),
                                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        return answer == QMessageBox::Yes ? path : QString();
    }

    // The patched copy is named after the game and the patch, so that its saves and states are its own.
    m_patchedGame = std::make_unique<QTemporaryDir>();
    QString target = m_patchedGame->filePath(QString("%1 [%2].%3").arg(QFileInfo(path).completeBaseName(), QFileInfo(patchFile).completeBaseName(), extension));
    QFile out(target);
    if (!m_patchedGame->isValid() || !out.open(QIODevice::WriteOnly) || out.write(result.data) != result.data.size()) {
        QMessageBox::warning(this, "Xfusion", "A temporary file for the patched game could not be written.");
        m_patchedGame.reset();
        return QString();
    }
    m_patchName = patchName;
    m_patchKind = result.format == PatchFormat::Ips ? "IPS" : result.format == PatchFormat::Bps ? "BPS" : "UPS";
    return target;
}

bool MainWindow::closeGame()
{
    if (!m_core.hasGame())
        return true;

    // The resume state is taken while the core is still running; the thread is stopped right after.
    if (m_settings.saveStateOnClose && m_statesSupported && m_emu) {
        QByteArray state;
        QString error;
        if (captureState(&state, &error)) {
            if (!m_saves->claim(&error) || !writeState(m_saves->resumeStatePath(), state))
                QMessageBox::warning(this, "Xfusion", "Could not save the state for the next time: " + error);
        }
    }

    trackPlayTime(false);
    commitPlayTime();
    m_saveTimer.stop();
    m_emu.reset();
    flushSaveRam(true);
    m_core.unloadGame();
    m_discSet.clear();
    m_patchedGame.reset();
    m_audio.reset();
    m_saves.reset();
    m_gameSystem.reset();
    m_statesSupported = false;
    m_input.setSwapped(false);
    m_manualPause = false;
    m_video->clear();
    m_video->osd()->setIndicators(false, false, false);
    m_central->setCurrentWidget(m_library);
    m_library->focusList();
    setWindowTitle("Xfusion");
    updateActions();
    return true;
}

void MainWindow::resetGame(bool powerCycle)
{
    if (!m_emu)
        return;
    maybeSaveOops();
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    if (powerCycle) {
        // Power cycling is closing and opening the game again: the core is reloaded and the saves are kept.
        const QString path = m_gamePath;
        openGame(path);
        showMessage("Power cycle");
        return;
    }
    m_emu->withCore([this] { m_core.reset(); });
    if (m_manualPause)
        setManualPause(false);
    showMessage("Reset");
}

void MainWindow::showRomInfo()
{
    if (!m_saves || !m_gameSystem)
        return;
    const QFileInfo info(m_gamePath);
    GameHeader header;
    QString extra;
    if (*m_gameSystem == System::SegaCD) {
        const DiscInfo disc = readDisc(m_gamePath);
        header = disc.header;
        extra = QString("Disc: %1, %2 data / %3 audio tracks\n").arg(disc.format).arg(disc.dataTracks).arg(disc.audioTracks);
        if (!disc.problems.isEmpty())
            extra += "Problems: " + disc.problems.join("; ") + "\n";
    } else {
        header = readCartridgeHeader(m_gamePath);
    }
    if (!m_patchName.isEmpty())
        extra += "Patch applied: " + m_patchName + "\n";
    if (header.valid) {
        extra += QString("Name in game: %1\nProduct: %2\nRegion: %3\n").arg(header.title(), header.product, header.regionText().isEmpty() ? QString("not stated") : header.regionText());
    }
    QMessageBox::information(this, "ROM Information",
                             extra + QString("Title: %1\nFile: %2\nSize: %3 bytes\nSystem: %4\nCore: %5\nGame id: %6\nSaves: %7\nStates: %8")
                                 .arg(info.completeBaseName(), QDir::toNativeSeparators(m_gamePath))
                                 .arg(info.size())
                                 .arg(systemDef(*m_gameSystem).folder, m_core.name(), m_saves->gameId(),
                                      QDir::toNativeSeparators(m_saves->saveRamPath()), m_statesSupported ? "supported" : "not supported by this core"));
}

void MainWindow::takeScreenshot()
{
    if (!m_emu || !m_gameSystem || !m_saves)
        return;
    const QImage frame = m_video->currentFrame();
    if (frame.isNull()) {
        showMessage("There is no picture to save yet");
        return;
    }
    const QString folder = AppPaths::screenshotsDir(*m_gameSystem, m_romDir);
    if (!QDir().mkpath(folder)) {
        QMessageBox::warning(this, "Xfusion", QString("Failed to take screenshot: cannot create %1").arg(folder));
        return;
    }
    const QString stem = QString("%1-%2").arg(m_saves->baseName(), QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss"));
    QString path = folder + "/" + stem + ".png";
    for (int i = 0; QFile::exists(path) && i < 1000; ++i)
        path = QString("%1/%2-%3.png").arg(folder, stem).arg(i);
    if (!frame.save(path, "PNG")) {
        QMessageBox::warning(this, "Xfusion", QString("Failed to take screenshot: %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    showMessage(QString("Saved screenshot %1").arg(QFileInfo(path).fileName()));
}

// A save file that cannot be used as it is stays untouched for the whole session: writing the core's own RAM
// over it at exit would destroy the only copy.
void MainWindow::loadSaveRam()
{
    QByteArray saved;
    QString error;
    switch (m_saves->readSaveRam(&saved, &error)) {
    case GameSaves::ReadResult::Missing:
        return;
    case GameSaves::ReadResult::Failed:
        m_saveRamLocked = true;
        QMessageBox::warning(this, "Xfusion", error + "\n\nThe game starts without its save and the file will not be overwritten in this session.");
        return;
    case GameSaves::ReadResult::Loaded:
        break;
    }

    QString warning;
    const bool loaded = m_core.setSaveRam(saved, &warning);
    if (loaded && warning.isEmpty()) {
        const qsizetype expected = m_core.profile().saveRamSizeVaries ? saved.size() : m_core.saveRam().size();
        if (expected > saved.size()) {
            // A save from another emulator that stores less. Keep the original next to it, once, before the first
            // write grows the file to the size this core uses.
            const QString original = m_saves->saveRamPath() + ".orig";
            if (!QFile::exists(original))
                QFile::copy(m_saves->saveRamPath(), original);
            showMessage(QString("Save file had %1 of %2 bytes; the rest starts empty (original kept as %3)")
                            .arg(saved.size()).arg(expected).arg(QFileInfo(original).fileName()));
            return; // m_lastSaveRam stays empty, so the full-size save is written at the next flush
        }
        m_lastSaveRam = saved;
        return;
    }
    m_saveRamLocked = true;
    QMessageBox::warning(this, "Xfusion",
                         QString("%1 was not loaded: %2.\n\nThe file will not be overwritten in this session.").arg(m_saves->saveRamPath(), warning));
}

// Writes the battery RAM when it changed. Closing the game always reports a failure; while playing, the first
// failure is reported and the write is retried quietly until it works.
void MainWindow::flushSaveRam(bool closing)
{
    if (!m_saves || m_saveRamLocked)
        return;

    QByteArray current;
    if (m_emu)
        m_emu->withCore([&] { current = m_core.saveRam(); });
    else
        current = m_core.saveRam();
    // PicoDrive reports no battery RAM while it is all zero, so a game that clears its own save looks like "none".
    // Once a save has existed, an empty report means it was cleared.
    if (current.isEmpty() && !m_lastSaveRam.isEmpty())
        current = QByteArray(m_lastSaveRam.size(), m_core.profile().saveRamSizeVaries ? char(0xFF) : char(0)); // Genesis Plus GX clears to 0xFF
    if (current.isEmpty() || current == m_lastSaveRam)
        return;

    QString error;
    if (m_saves->writeSaveRam(current, &error)) {
        m_lastSaveRam = current;
        m_saveWriteFailing = false;
        return;
    }
    if (closing || !m_saveWriteFailing) {
        QMessageBox::warning(this, "Xfusion",
                             error + (closing ? "\n\nThe progress since the last successful save is lost."
                                              : "\n\nXfusion will keep trying every few seconds."));
    }
    m_saveWriteFailing = true;
}

void MainWindow::saveSramNow()
{
    if (!m_emu || !m_saves)
        return;
    flushSaveRam(false);
    if (QFile::exists(m_saves->saveRamPath()))
        showMessage(QString("Saved S-RAM %1").arg(QFileInfo(m_saves->saveRamPath()).fileName()));
    else
        showMessage("This game has nothing to save in S-RAM yet");
}

// ------------------------------------------------------------------------------------------------- states

bool MainWindow::stateOperationsAvailable() const
{
    return m_emu && m_saves && m_gameSystem && m_statesSupported;
}

QString MainWindow::slotDescription(int slot) const
{
    if (!m_saves)
        return QString("Current slot: %1").arg(slot);
    return QString("Current slot: %1 [%2]").arg(slot).arg(QFile::exists(m_saves->statePath(slot)) ? "used" : "empty");
}

void MainWindow::setCurrentSlot(int slot)
{
    const int count = GameSaves::kMaxSlot + 1;
    m_currentSlot = ((slot % count) + count) % count;
    showMessage(slotDescription(m_currentSlot));
}

bool MainWindow::captureState(QByteArray* state, QString* error)
{
    if (!m_emu)
        return false;
    bool ok = false;
    m_emu->withCore([&] { ok = m_core.serialize(state, error); });
    return ok;
}

bool MainWindow::writeState(const QString& path, const QByteArray& state)
{
    StateHeader header;
    header.coreId = m_core.profile().id;
    header.coreName = m_core.name();
    header.gameId = m_saves->gameId();
    header.gameName = m_saves->baseName();
    header.system = systemDef(*m_gameSystem).folder;
    header.created = QDateTime::currentDateTimeUtc();
    header.rawSize = state.size();
    QImage thumbnail = m_video->currentFrame();
    if (!thumbnail.isNull())
        thumbnail = thumbnail.scaledToWidth(kThumbnailWidth, Qt::SmoothTransformation);
    QString error;
    if (StateFile::write(path, header, state, thumbnail, &error))
        return true;
    QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
    return false;
}

void MainWindow::saveStateToSlot(int slot, bool confirm)
{
    if (!stateOperationsAvailable()) {
        if (m_emu)
            showMessage("This core does not support save states");
        return;
    }
    QString error;
    if (!m_saves->claim(&error)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        return;
    }
    saveStateToFile(m_saves->statePath(slot), confirm ? slot : -1);
}

void MainWindow::saveStateToFile(const QString& path, int confirmSlot)
{
    if (!stateOperationsAvailable())
        return;
    QString error;
    if (!m_saves->claim(&error)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        return;
    }
    if (confirmSlot >= 0 && m_settings.confirmSaveLoad && QFile::exists(path)
        && QMessageBox::question(this, "Confirm Save", QString("Overwrite existing game position in slot #%1?").arg(confirmSlot)) != QMessageBox::Yes)
        return;

    QByteArray state;
    if (!captureState(&state, &error)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        showMessage("Failed to save " + QFileInfo(path).fileName());
        return;
    }
    if (!writeState(path, state)) {
        showMessage("Failed to save " + QFileInfo(path).fileName());
        return;
    }
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    showMessage(QFileInfo(path).fileName() + " saved");
}

void MainWindow::loadStateFromSlot(int slot, bool confirm)
{
    if (!stateOperationsAvailable()) {
        if (m_emu)
            showMessage("This core does not support save states");
        return;
    }
    const QString path = m_saves->statePath(slot);
    if (!QFile::exists(path)) {
        showMessage("Failed to load " + QFileInfo(path).fileName());
        return;
    }
    if (confirm && m_settings.confirmSaveLoad
        && QMessageBox::question(this, "Confirm Load", QString("Load game position from slot #%1?").arg(slot)) != QMessageBox::Yes)
        return;
    loadStateFromFile(path);
}

void MainWindow::loadOopsState()
{
    if (!stateOperationsAvailable())
        return;
    if (!QFile::exists(m_saves->oopsStatePath())) {
        QMessageBox::warning(this, "Xfusion", "No game position file available.");
        return;
    }
    loadStateFromFile(m_saves->oopsStatePath());
}

void MainWindow::loadUndoState()
{
    if (!stateOperationsAvailable())
        return;
    const QString path = m_saves->undoStatePath();
    if (!QFile::exists(path)) {
        return;
    }
    QString error;
    const auto state = StateFile::read(path, &error);
    if (!state) {
        QMessageBox::warning(this, "Xfusion", QString("Could not undo: %1").arg(error));
        return;
    }
    // Undo goes back without making a new undo point, so it can be repeated.
    applyState(state->raw, QString(), false);
}

void MainWindow::maybeSaveOops()
{
    if (!stateOperationsAvailable() || QDateTime::currentMSecsSinceEpoch() - m_lastStateActivityMs < kOopsAfterMs)
        return;
    QByteArray state;
    QString error;
    if (captureState(&state, &error) && m_saves->claim(&error) && writeState(m_saves->oopsStatePath(), state))
        m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::loadStateFromFile(const QString& path)
{
    if (!stateOperationsAvailable())
        return;
    QString error;
    const auto state = StateFile::read(path, &error);
    if (!state) {
        QMessageBox::warning(this, "Xfusion", QString("Could not load %1: %2").arg(QFileInfo(path).fileName(), error));
        showMessage("Failed to load " + QFileInfo(path).fileName());
        return;
    }
    if (state->header.gameId != m_saves->gameId()) {
        QMessageBox::warning(this, "Xfusion", QString("%1 belongs to a different game and was not loaded.").arg(QFileInfo(path).fileName()));
        return;
    }
    if (state->header.coreId != m_core.profile().id) {
        QMessageBox::warning(this, "Xfusion",
                             QString("%1 was saved with the core \"%2\", not this one, and cannot be loaded.").arg(QFileInfo(path).fileName(), state->header.coreId));
        return;
    }
    if (state->header.coreName != m_core.name()) {
        const auto answer = QMessageBox::question(
            this, "Xfusion",
            QString("%1 was saved with %2, but the core is now %3.\n\nStates are not guaranteed to work across core versions: "
                    "the game may glitch or crash. You can undo the load.\n\nLoad it anyway?")
                .arg(QFileInfo(path).fileName(), state->header.coreName, m_core.name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    maybeSaveOops();
    applyState(state->raw, QFileInfo(path).fileName() + " loaded", true);
}

// Puts a state into the core. A load first writes the current game to the undo file; if the core rejects the state
// it is put back as it was.
bool MainWindow::applyState(const QByteArray& state, const QString& what, bool makeUndo)
{
    QByteArray before;
    QString error;
    QString restoreError;
    bool loaded = false;
    bool restored = true;
    bool haveBefore = false;
    const bool paused = m_manualPause || m_focusPause || m_minimizedPause || m_dialogPause > 0;
    m_emu->withCore([&] {
        haveBefore = m_core.serialize(&before, &error);
        loaded = m_core.unserialize(state, &error);
        if (!loaded && haveBefore)
            restored = m_core.unserialize(before, &restoreError);
        // A paused game would keep showing the old picture; one frame draws the restored one.
        if (loaded && paused)
            m_core.run();
    });

    if (!loaded) {
        QMessageBox::warning(this, "Xfusion",
                             QString("Could not load the state: %1.\n\n%2")
                                 .arg(error, restored ? "The game was left as it was."
                                                      : "The game could not be put back as it was; reset it or reopen the game."));
        return false;
    }
    if (makeUndo && haveBefore) {
        QString claimError;
        if (m_saves->claim(&claimError))
            writeState(m_saves->undoStatePath(), before);
    }
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    if (!what.isEmpty())
        showMessage(what);
    return true;
}

void MainWindow::showStatePreview(bool saving)
{
    if (!stateOperationsAvailable())
        return;
    StatePreviewDialog dialog(*m_saves, saving, m_currentSlot / 10, this);
    connect(&dialog, &StatePreviewDialog::slotChosen, this, [this, saving, &dialog](int slot) {
        if (saving) {
            saveStateToSlot(slot, true);
            dialog.refresh();
        } else {
            loadStateFromSlot(slot, true);
            dialog.accept();
        }
    });
    dialog.exec();
}

// ------------------------------------------------------------------------------------------------ settings

void MainWindow::openSettings(int page)
{
    // The game would keep running behind the dialog, so it is held while the settings are open.
    ++m_dialogPause;
    updatePause();

    SettingsDialog dialog(m_input, m_input.config(), m_settings, m_inputGroup, static_cast<SettingsDialog::Page>(page), this);
    const int dialogResult = dialog.exec();
    if (dialogResult == QDialog::Accepted) {
        const InputConfig config = dialog.inputConfig();
        m_input.setConfig(config);
        if (m_emu)
            m_emu->withCore([this, &config] { m_core.setPads(config.padType(), config.portMode() == InputConfig::PortMode::TwoControllers, m_inputGroup == InputGroup::Genesis); });
        else
            m_core.setPads(config.padType(), config.portMode() == InputConfig::PortMode::TwoControllers, m_inputGroup == InputGroup::Genesis);
        m_settings = dialog.appSettings();
        applySettings();

        QString error;
        if (!config.save(AppPaths::inputConfigPath(), &error) || !m_settings.save(AppPaths::settingsPath(), &error))
            QMessageBox::warning(this, "Xfusion", error);
    }

    --m_dialogPause;
    updatePause();
}

void MainWindow::applySettings()
{
    static int appliedTheme = -1;
    if (appliedTheme != m_settings.theme) {
        Theme::apply(m_settings.theme);
        refreshThemedIcons(this);
        appliedTheme = m_settings.theme;
    }
    AppPaths::setFiles(m_settings);
    AppPaths::patchFolders(System::Genesis, QString()); // creates the default patches folder, so that it is there to find

    DisplayOptions display;
    display.aspect = m_settings.aspect;
    display.maintainAspect = m_settings.maintainAspect;
    display.stretch = m_settings.stretch;
    display.integerScale = m_settings.integerScale;
    m_video->setDisplay(display, m_settings.smooth, m_settings.filter);

    OsdOverlay::Options osd;
    osd.messages = m_settings.messages;
    osd.size = m_settings.osdSize;
    osd.corner = m_settings.osdCorner;
    osd.showFps = m_settings.showFps;
    osd.showIndicators = m_settings.showIndicators;
    osd.showTime = m_settings.showTime;
    osd.showPressed = m_settings.showPressedKeys;
    m_video->osd()->setOptions(osd);

    m_library->setFolders(m_settings.libraryFolders);
    rebuildJoystickShortcuts();
    refreshMenuHints();
    m_pauseUnfocusedAction->setChecked(m_settings.pauseWhenUnfocused);
    if (m_emu) {
        m_saveTimer.stop();
        if (m_settings.sramInterval > 0)
            m_saveTimer.start(m_settings.sramInterval * 1000);
        updateSpeed();
    }
    updateAudioMode();
    updatePause();
}

// Volume, mute, speed and frame skip are changed from the keyboard, so they are saved right away and shown over the
// picture: the status bar is hidden in fullscreen.
void MainWindow::commitSettings(const QString& message)
{
    applySettings();
    QString error;
    if (m_settings.save(AppPaths::settingsPath(), &error))
        showMessage(message);
    else
        showMessage(error);
}

void MainWindow::adjustVolume(int delta)
{
    m_settings.volume = std::clamp(m_settings.volume + delta, 0, AppSettings::kMaxVolume);
    if (delta > 0)
        m_settings.muted = false;
    commitSettings(m_settings.muted ? QString("Muted") : QString("Volume %1%").arg(m_settings.volume));
}

void MainWindow::toggleMute()
{
    m_settings.muted = !m_settings.muted;
    commitSettings(m_settings.muted ? QString("Muted") : QString("Volume %1%").arg(m_settings.volume));
}

void MainWindow::changeSpeed(int direction)
{
    if (!m_emu)
        return;
    const double native = m_core.avInfo().fps;
    const double step = native * 0.1;
    const double current = m_settings.fixedFrameRate > 0 ? m_settings.fixedFrameRate : native;
    m_settings.fixedFrameRate = std::clamp(current + direction * step, step, native * 10);
    commitSettings(QString("Speed: %1%").arg(static_cast<int>(m_settings.fixedFrameRate / native * 100)));
}

void MainWindow::changeFrameSkip(int delta)
{
    m_settings.fixedFrameSkip = std::clamp(m_settings.fixedFrameSkip + delta, 0, 59);
    commitSettings(QString("Frame skip: %1").arg(m_settings.fixedFrameSkip));
}

SpeedConfig MainWindow::speedConfig() const
{
    SpeedConfig config;
    config.method = m_settings.speedMethod;
    const double native = m_core.avInfo().fps;
    config.ratio = m_settings.fixedFrameRate > 0 && native > 0 ? std::clamp(m_settings.fixedFrameRate / native, 0.1, 10.0) : 1.0;
    config.maxFrameSkip = m_settings.maxFrameSkip;
    config.fastForwardSkip = m_settings.fastForwardSkip;
    config.fixedFrameSkip = m_settings.fixedFrameSkip;
    config.bufferMs = m_settings.bufferMs;
    return config;
}

void MainWindow::updateSpeed()
{
    if (m_emu)
        m_emu->setSpeed(speedConfig());
}

// Fast-forward and rewind change the sound: the volume, whether it plays at all, and (in the emulation thread) its pitch.
void MainWindow::updateAudioMode()
{
    const bool fastForward = m_fastForwardHeld || m_fastForwardToggled;
    if (m_emu)
        m_emu->setFastForward(fastForward);
    if (m_audio)
        m_audio->setGain(m_settings.gain(fastForward));
    m_muteFastForward = m_settings.muteDuringFastForward;
    m_muteStepping = m_settings.muteDuringFrameAdvance;
}

void MainWindow::trackPlayTime(bool running)
{
    if (running == m_playRunning)
        return;
    if (running)
        m_playClock.start();
    else
        m_playPendingMs += m_playClock.elapsed();
    m_playRunning = running;
}

void MainWindow::commitPlayTime()
{
    qint64 duration = m_playPendingMs + (m_playRunning ? m_playClock.restart() : 0);
    m_playPendingMs = 0;
    if (duration > 0 && m_saves && !m_libraryId.isEmpty())
        m_library->recordSession(m_libraryId, duration);
}

void MainWindow::setManualPause(bool paused)
{
    m_manualPause = paused;
    updatePause();
}

void MainWindow::updatePause()
{
    const bool paused = m_manualPause || m_focusPause || m_minimizedPause || m_dialogPause > 0;
    if (m_emu)
        m_emu->setPaused(paused);
    trackPlayTime(m_emu && !paused);
    m_actionTexts[ShortcutAction::PauseContinue] = m_manualPause ? "&Pause \u2713" : "&Pause";
    refreshMenuHints();
    updateOverlays();
}

void MainWindow::toggleFullscreen()
{
    const bool full = !isFullScreen();
    menuBar()->setVisible(!full);
    statusBar()->setVisible(!full);
    if (full)
        showFullScreen();
    else
        showNormal();
}

void MainWindow::resizeToMultiple(int multiple)
{
    if (isFullScreen())
        toggleFullscreen();
    const QImage frame = m_video->currentFrame();
    const int frameHeight = frame.isNull() ? 224 : frame.height();
    const double aspect = m_core.avInfo().aspectRatio > 0 ? m_core.avInfo().aspectRatio : 4.0 / 3.0;
    const QSize video(int(frameHeight * multiple * aspect), frameHeight * multiple);
    const QSize chrome = size() - m_video->size();
    resize(video + chrome);
}

void MainWindow::updateActions()
{
    const bool playing = m_core.hasGame();
    for (QAction* action : m_gameActions)
        action->setEnabled(playing);
    for (QAction* action : m_stateActions)
        action->setEnabled(playing && m_statesSupported);
    m_saveMenu->setEnabled(playing && m_statesSupported);
    m_loadMenu->setEnabled(playing && m_statesSupported);
    m_discMenu->menuAction()->setVisible(playing && m_discSet.size() > 1);
}

// A game on several discs: the core has the disc that was opened and the others are added behind it, so that the
// Disc menu can swap them while the game runs.
void MainWindow::setupDiscs(const QString& path)
{
    m_discSet = {path};
    m_discCoreIndex = {0};
    m_currentDisc = 0;
    if (m_core.discCount() == 0) // the core has no disk control
        return;
    const QStringList set = discSetOf(path);
    if (set.size() < 2)
        return;
    const QString opened = QFileInfo(path).absoluteFilePath();
    m_discSet.clear();
    m_discCoreIndex.clear();
    for (const QString& disc : set) {
        if (disc == opened) {
            m_currentDisc = int(m_discSet.size());
            m_discSet << disc;
            m_discCoreIndex.push_back(0);
        } else if (m_core.addDisc(disc)) {
            m_discSet << disc;
            m_discCoreIndex.push_back(m_core.discCount() - 1);
        }
    }
}

void MainWindow::populateDiscMenu()
{
    m_discMenu->clear();
    auto* group = new QActionGroup(m_discMenu);
    for (int i = 0; i < m_discSet.size(); ++i) {
        QAction* action = m_discMenu->addAction(QString("Disc %1: %2").arg(i + 1).arg(QFileInfo(m_discSet[i]).completeBaseName()), this, [this, i] { swapToDisc(i); });
        action->setCheckable(true);
        action->setChecked(i == m_currentDisc);
        group->addAction(action);
    }
}

void MainWindow::swapToDisc(int setIndex)
{
    if (!m_emu || setIndex < 0 || setIndex >= m_discSet.size() || setIndex == m_currentDisc)
        return;
    bool swapped = false;
    m_emu->withCore([&] { swapped = m_core.swapDisc(m_discCoreIndex[size_t(setIndex)]); });
    if (swapped) {
        m_currentDisc = setIndex;
        setWindowTitle(QString("Xfusion - %1").arg(QFileInfo(m_discSet[setIndex]).completeBaseName()));
        showMessage(QString("Disc %1: %2").arg(setIndex + 1).arg(QFileInfo(m_discSet[setIndex]).completeBaseName()));
    } else {
        showMessage(QString("The core could not change to disc %1").arg(setIndex + 1));
    }
}

void MainWindow::showMessage(const QString& text)
{
    m_video->osd()->post(text);
}

// Runs on the emulation thread. Reading the physical state each frame avoids losing press/release events
// after dialogs or focus changes, and keeps input independent of the GUI thread.
void MainWindow::pollInput()
{
    const InputManager::PadStates states = m_input.poll(m_inputGroup, m_windowActive, m_backgroundPads);
    ++m_turboFrame;
    const bool turboOn = (m_turboFrame / 2) % 2 == 0; // four frames per cycle: 15 presses a second
    for (std::size_t player = 0; player < states.size(); ++player) {
        for (std::size_t i = 0; i < kConsoleButtonCount; ++i) {
            bool pressed = states[player][i];
            // A, B, C, X, Y and Z have a Turbo entry eight places on.
            if (i >= static_cast<std::size_t>(PadButton::A) && i <= static_cast<std::size_t>(PadButton::Z))
                pressed = pressed || (turboOn && states[player][i + 8]);
            m_core.setButton(static_cast<int>(player), static_cast<PadButton>(i), pressed);
        }
    }
    unsigned mask = 0;
    for (std::size_t i = 0; i < kConsoleButtonCount; ++i)
        if (states[0][i])
            mask |= 1u << i;
    m_pressedMask = mask;
}

void MainWindow::enableStats()
{
    m_statsTimer.setInterval(1000);
    connect(&m_statsTimer, &QTimer::timeout, this, &MainWindow::logStats);
    m_statsTimer.start();
}

void MainWindow::logStats()
{
    if (!m_emu || !m_audio)
        return;
    const AudioOutput::Stats audio = m_audio->takeStats();
    std::fprintf(stderr, "[stats] frames=%d underruns=%u audioQueue=%d..%d bytes\n", m_emu->takeFrameCount(),
                 audio.underruns, audio.minQueuedBytes, audio.maxQueuedBytes);
}

void MainWindow::updateOverlays()
{
    OsdOverlay* osd = m_video->osd();
    const bool paused = m_emu && (m_manualPause || m_focusPause || m_minimizedPause || m_dialogPause > 0);
    osd->setIndicators(paused, m_emu && m_emu->fastForward(), m_emu && m_emu->rewinding());
    if (!m_emu) {
        osd->setFps(0);
        osd->setPressedText({});
        return;
    }
    // The frame rate is averaged over about a second.
    static qint64 windowStart = 0;
    static int frames = 0;
    frames += m_emu->takeFpsFrames();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - windowStart >= 1000) {
        osd->setFps(windowStart == 0 ? 0 : frames * 1000.0 / double(now - windowStart));
        windowStart = now;
        frames = 0;
    }
    if (m_settings.showPressedKeys) {
        QStringList names;
        const unsigned mask = m_pressedMask;
        for (const InputAction& action : inputGroupDef(m_inputGroup).actions)
            if (!isTurbo(action.button) && (mask & (1u << static_cast<unsigned>(action.button))))
                names << action.label;
        osd->setPressedText(names.join(" + "));
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
