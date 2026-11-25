#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <cmath>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QStatusBar>

#include <algorithm>
#include <cstdio>

#include "Achievements.h"
#include "AchievementsToken.h"
#include "AppPaths.h"
#include "GpuFilters.h"
#include "ShaderParametersDialog.h"
#include "DiscInfo.h"
#include "Patch.h"
#include "OsdOverlay.h"
#include "SettingsDialog.h"
#include "Icons.h"
#include "LibraryPage.h"
#include "Theme.h"

// ------------------------------------------------------------------------------------------------ settings

void MainWindow::openSettings(int page)
{
    // The game would keep running behind the dialog, so it is held while the settings are open.
    ++m_dialogPause;
    updatePause();

    SettingsDialog dialog(m_input, m_input.config(), m_settings, m_inputGroup, static_cast<SettingsDialog::Page>(page), m_achievements, this);
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

    applyView();

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
    updateAchievementsAccount();
}

std::shared_ptr<const PresetProgram> MainWindow::presetFor(const QString& path)
{
    if (path.isEmpty()) {
        if (!m_presets.contains(QString()))
            m_video->osd()->post("No shader preset is chosen (Options > Display)", 4000);
        m_presets[QString()] = nullptr;
        return nullptr;
    }
    auto found = m_presets.find(path);
    if (found == m_presets.end()) {
        // The presets inside the program (the filters of the GPU) are loaded without a word unless they fail.
        const bool ours = path.startsWith(":/");
        if (!ours) {
            m_video->osd()->post(QString("Loading shader %1...").arg(QFileInfo(path).fileName()), 3000);
            QApplication::processEvents(); // so that the message is seen while the shaders compile
        }
        std::shared_ptr<PresetProgram> program = loadPresetProgram(path, AppPaths::shaderCacheDir());
        found = m_presets.emplace(path, program).first;
        if (!program->error.isEmpty()) {
            m_video->osd()->post((ours ? "GPU filter: " : "Shader preset: ") + program->error.left(240), 7000);
            statusBar()->showMessage("Shader preset: " + program->error, 15000);
        } else if (!ours) {
            m_video->osd()->post(QString("Shader: %1").arg(QFileInfo(path).completeBaseName()), 3000);
        }
    }
    return found->second && found->second->error.isEmpty() ? found->second : nullptr;
}

void MainWindow::openShaderParameters()
{
    if (!m_activePreset || m_activePreset->parameters.isEmpty())
        return;
    const std::shared_ptr<const PresetProgram> program = m_activePreset;
    QMap<QString, double> current = program->defaults;
    const QMap<QString, double> saved = m_settings.shaderParameters.value(program->path);
    for (auto it = saved.begin(); it != saved.end(); ++it)
        if (current.contains(it.key()))
            current[it.key()] = it.value();
    ShaderParametersDialog dialog(QString("Shader parameters - %1").arg(QFileInfo(program->path).completeBaseName()), program->parameters, current, this);
    connect(&dialog, &ShaderParametersDialog::changed, this, [this](const QMap<QString, double>& values) { m_video->setShaderParameters(values); });
    if (dialog.exec() != QDialog::Accepted) {
        m_video->setShaderParameters(current);
        return;
    }
    // Only what differs from the preset is kept, so that a preset updated later still shows through.
    QMap<QString, double> changed;
    const QMap<QString, double> now = dialog.values();
    for (auto it = now.begin(); it != now.end(); ++it)
        if (std::fabs(it.value() - program->defaults.value(it.key())) > 1e-9)
            changed[it.key()] = it.value();
    if (changed.isEmpty())
        m_settings.shaderParameters.remove(program->path);
    else
        m_settings.shaderParameters[program->path] = changed;
    commitSettings("Shader parameters saved");
}

void MainWindow::applyView()
{
    m_view = m_gameSystem ? m_settings.forSystem(*m_gameSystem) : m_settings;

    DisplayOptions display;
    display.aspect = m_view.aspect;
    display.maintainAspect = m_view.maintainAspect;
    display.stretch = m_view.stretch;
    display.integerScale = m_view.integerScale;
    VideoShader shader = m_view.shader;
    VideoFilter filter = m_view.filter;
    std::shared_ptr<const PresetProgram> preset;
    bool userPreset = false;
    if (shader == VideoShader::Preset) {
        if (m_video->usesGpu())
            preset = presetFor(m_view.shaderPreset);
        userPreset = preset != nullptr;
        if (!preset)
            shader = VideoShader::None;
    } else if (shader == VideoShader::None && m_view.gpuFilters && m_video->usesGpu() && filter != VideoFilter::None) {
        // A filter with a version for the GPU is done there. With a shader on top the CPU does it, since a shader cannot follow it.
        if (filter == VideoFilter::Scanlines) {
            shader = VideoShader::Scanlines;
            filter = VideoFilter::None;
        } else if (const QString path = gpuFilterPreset(filter); !path.isEmpty()) {
            preset = presetFor(path);
            if (preset)
                filter = VideoFilter::None;
        }
    }
    m_video->setDisplay(display, m_view.smooth, filter, shader);
    QMap<QString, double> values;
    if (preset) {
        values = preset->defaults;
        const QMap<QString, double> mine = m_settings.shaderParameters.value(preset->path);
        for (auto it = mine.begin(); it != mine.end(); ++it)
            if (values.contains(it.key()))
                values[it.key()] = it.value();
    }
    if (preset != m_activePreset) {
        m_activePreset = preset;
        m_video->setShaderPreset(preset, values);
    } else {
        m_video->setShaderParameters(values);
    }
    if (m_shaderParametersAction)
        m_shaderParametersAction->setEnabled(userPreset && !preset->parameters.isEmpty());

    OsdOverlay::Options osd;
    osd.messages = m_view.messages;
    osd.size = m_view.osdSize;
    osd.corner = m_view.osdCorner;
    osd.showFps = m_settings.showFps;
    osd.showIndicators = m_settings.showIndicators;
    osd.showTime = m_settings.showTime;
    osd.showPressed = m_settings.showPressedKeys;
    m_video->osd()->setOptions(osd);
    updateAudioMode();
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
    m_settings.setSoundValue(m_gameSystem, "volume", std::clamp(m_view.volume + delta, 0, AppSettings::kMaxVolume));
    if (delta > 0)
        m_settings.setSoundValue(m_gameSystem, "muted", false);
    applyView();
    commitSettings(m_view.muted ? QString("Muted") : QString("Volume %1%").arg(m_view.volume));
}

void MainWindow::toggleMute()
{
    m_settings.setSoundValue(m_gameSystem, "muted", !m_view.muted);
    applyView();
    commitSettings(m_view.muted ? QString("Muted") : QString("Volume %1%").arg(m_view.volume));
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
    config.bufferMs = m_view.bufferMs;
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
        m_audio->setGain(m_view.gain(fastForward));
    m_muteFastForward = m_view.muteDuringFastForward;
    m_muteStepping = m_view.muteDuringFrameAdvance;
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
    m_discMenu->menuAction()->setVisible(playing && !m_discSet.isEmpty());
}

// A game on several discs: the core has the disc that was opened and the others are added behind it, so that the
// Disc menu can swap them while the game runs.
// RetroAchievements identifies a game by the hash of its contents: a cartridge by the file (read from a zip when it is
// there), a disc by its image (.cue, .iso or .chd). A game with a patch applied is not the game they know.
void MainWindow::loadAchievements(System system, const QString& path, bool isDisc)
{
    if (!m_settings.achievements.enabled)
        return;
    m_achievementsTried = true;
    if (!m_patchKind.isEmpty()) {
        showMessage("Achievements are off: the game has a patch applied");
        return;
    }
    QByteArray data;
    if (!isDisc) {
        QString extension, error;
        if (!readGameBytes(path, &data, &extension, &error))
            return;
    }
    m_achievements->loadGame(system, path, data, m_core);
}

// Signs in with the saved account when achievements are on and nobody is signed in.
void MainWindow::updateAchievementsAccount()
{
    m_achievements->configure(m_settings.achievements);
    const AchievementsSettings& a = m_settings.achievements;
    if (!a.enabled) {
        m_achievements->unloadGame(); // the session ends with the program; the game stops being tracked
        m_achievementsTried = false;
        return;
    }
    if (!m_achievements->user().loggedIn && !m_achievements->loginPending() && !a.username.isEmpty()) {
        const QString token = decryptToken(a.username, a.token);
        if (!token.isEmpty())
            m_achievements->loginWithToken(a.username, token);
    }
    // Turned on (or signed in) with a game already open: it is offered now, once, instead of at the next game.
    if (m_emu && m_gameSystem && !m_achievementsTried)
        loadAchievements(*m_gameSystem, m_gamePath, *m_gameSystem == System::SegaCD);
}

void MainWindow::setupDiscs(const QString& path)
{
    m_discSet.clear();
    m_discCoreIndex.clear();
    m_currentDisc = 0;
    if (m_core.discCount() == 0) // the core has no disk control
        return;
    m_discSet = {path};
    m_discCoreIndex = {0};
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
    // Like the Change Disc menu of DuckStation: the discs of the game, next and previous, and any disc from a file.
    m_discMenu->addSeparator();
    QAction* next = m_discMenu->addAction("Switch to &Next Disc", this, [this] { switchDisc(1); });
    QAction* previous = m_discMenu->addAction("Switch to &Previous Disc", this, [this] { switchDisc(-1); });
    next->setEnabled(m_currentDisc + 1 < m_discSet.size());
    previous->setEnabled(m_currentDisc > 0);
    m_discMenu->addAction("Insert Disc From &File...", this, [this] { insertDiscFile(); });
}

void MainWindow::switchDisc(int direction)
{
    if (m_discSet.size() < 2) {
        showMessage("Current game does not have multiple discs.");
        return;
    }
    const int target = m_currentDisc + direction;
    if (target < 0 || target >= m_discSet.size()) {
        showMessage(direction < 0 ? "There is no previous disc to switch to." : "There is no next disc to switch to.");
        return;
    }
    swapToDisc(target);
}

// Any disc image can be put in the drive, whatever its name: it is added behind the ones the core has and inserted.
void MainWindow::insertDiscFile()
{
    if (!m_emu || m_discSet.isEmpty())
        return;
    ++m_dialogPause;
    updatePause();
    const QString chosen = QFileDialog::getOpenFileName(this, "Insert a disc", m_lastDir, "Discs (*.cue *.chd *.iso);;All files (*)");
    --m_dialogPause;
    updatePause();
    if (chosen.isEmpty())
        return;
    const QString path = QFileInfo(chosen).absoluteFilePath();
    for (int i = 0; i < m_discSet.size(); ++i) {
        if (QFileInfo(m_discSet[i]).absoluteFilePath() == path) {
            swapToDisc(i);
            return;
        }
    }
    bool added = false;
    m_emu->withCore([&] { added = m_core.addDisc(path); });
    if (!added) {
        showMessage("The core cannot hold another disc");
        return;
    }
    m_discSet << path;
    m_discCoreIndex.push_back(m_core.discCount() - 1);
    swapToDisc(int(m_discSet.size()) - 1);
}

void MainWindow::swapToDisc(int setIndex)
{
    if (!m_emu || setIndex < 0 || setIndex >= m_discSet.size() || setIndex == m_currentDisc)
        return;
    bool swapped = false;
    m_emu->withCore([&] { swapped = m_core.swapDisc(m_discCoreIndex[size_t(setIndex)]); });
    if (swapped) {
        m_currentDisc = setIndex;
        m_achievements->changeDisc(m_discSet[setIndex]);
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

