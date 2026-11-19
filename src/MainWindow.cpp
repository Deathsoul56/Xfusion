#include "MainWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include <cstdio>

#include "AppPaths.h"
#include "SettingsDialog.h"
#include "SystemInfo.h"

MainWindow::MainWindow(LibretroCore& core, QWidget* parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_video(new VideoWidget(this))
{
    setWindowTitle("Xfusion");
    setCentralWidget(m_video);
    setFocusPolicy(Qt::StrongFocus);
    resize(960, 720);

    m_core.setVideoCallback([this](const QImage& frame, double aspect) { m_video->submitFrame(frame, aspect); });
    m_core.setAudioCallback([this](const int16_t* samples, size_t frames) {
        if (m_audio)
            m_audio->push(samples, frames);
    });

    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* openAction = fileMenu->addAction("&Open Game...", QKeySequence::Open, this, &MainWindow::promptOpenGame);
    Q_UNUSED(openAction);
    m_closeAction = fileMenu->addAction("&Close Game", this, &MainWindow::closeGame);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);

    QMenu* emuMenu = menuBar()->addMenu("&Emulation");
    m_pauseAction = emuMenu->addAction("&Pause", QKeySequence(Qt::Key_P));
    m_pauseAction->setCheckable(true);
    connect(m_pauseAction, &QAction::toggled, this, &MainWindow::setPaused);
    m_resetAction = emuMenu->addAction("&Reset", QKeySequence("Ctrl+R"), this, &MainWindow::resetGame);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("&Fullscreen", QKeySequence(Qt::Key_F11), this, &MainWindow::toggleFullscreen);

    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    settingsMenu->addAction("&Controllers...", QKeySequence("Ctrl+,"), this, &MainWindow::openSettings);

    QString configError;
    m_input.setConfig(InputConfig::load(AppPaths::inputConfigPath(), &configError));
    statusBar()->showMessage(configError.isEmpty() ? QString("Core: %1 - open a game to start").arg(m_core.name()) : configError);
    updateActions();
}

void MainWindow::promptOpenGame()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open game", m_lastDir, romFileDialogFilter());
    if (!path.isEmpty())
        openGame(path);
}

void MainWindow::openGame(const QString& path)
{
    const std::optional<System> system = systemForFile(path);
    if (!system) {
        QMessageBox::warning(this, "Xfusion", QString("Unrecognized game format: %1").arg(path));
        return;
    }

    closeGame();
    m_lastDir = QFileInfo(path).absolutePath();

    const QString saveDir = AppPaths::savesDir(*system);
    m_core.setDirectories(AppPaths::systemDir(), saveDir);

    QString error;
    m_core.setPadType(m_input.config().padType());
    if (!m_core.loadGame(path, &error)) {
        if (*system == System::SegaCD) {
            error += QString("\n\nSega CD games need the BIOS for the disc's region in:\n%1\nExpected file names: %2")
                         .arg(AppPaths::systemDir(), m_core.profile().segaCdBiosFiles.join(", "));
        }
        QMessageBox::critical(this, "Xfusion", error);
        return;
    }

    m_saveRamPath = saveDir + "/" + QFileInfo(path).completeBaseName() + ".srm";
    m_core.readSaveRam(m_saveRamPath);

    m_audio = std::make_unique<AudioOutput>(m_core.avInfo().sampleRate);
    m_pauseAction->setChecked(false);
    m_inputGroup = systemDef(*system).inputGroup;
    m_emu = std::make_unique<EmuThread>(m_core, m_audio.get(), [this] { pollInput(); });
    m_emu->start();

    setWindowTitle(QString("Xfusion - %1").arg(QFileInfo(path).completeBaseName()));
    statusBar()->showMessage(QString("%1 | %2 | saves: %3").arg(m_core.name(), systemDef(*system).folder, saveDir));
    updateActions();
    setFocus();
}

void MainWindow::closeGame()
{
    if (!m_core.hasGame())
        return;

    m_emu.reset();
    if (!m_core.writeSaveRam(m_saveRamPath))
        QMessageBox::warning(this, "Xfusion", QString("Could not write save file: %1").arg(m_saveRamPath));
    m_core.unloadGame();
    m_audio.reset();
    m_video->clear();
    setWindowTitle("Xfusion");
    statusBar()->showMessage("Game closed");
    updateActions();
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

// Runs on the emulation thread. Reading the physical state each frame avoids losing press/release events
// after dialogs or focus changes, and keeps input independent of the GUI thread.
void MainWindow::pollInput()
{
    const InputManager::PadState state = m_input.poll(m_inputGroup, m_windowActive);
    for (std::size_t i = 0; i < state.size(); ++i)
        m_core.setButton(static_cast<PadButton>(i), state[i]);
}

void MainWindow::openSettings()
{
    // The game would keep running behind the dialog, so it is held while the settings are open.
    const bool pause = m_emu && !m_pauseAction->isChecked();
    if (pause)
        m_emu->setPaused(true);

    SettingsDialog dialog(m_input, m_input.config(), m_inputGroup, this);
    if (dialog.exec() == QDialog::Accepted) {
        const InputConfig config = dialog.inputConfig();
        m_input.setConfig(config);
        if (m_emu)
            m_emu->withCore([this, &config] { m_core.setPadType(config.padType()); });
        else
            m_core.setPadType(config.padType());
        QString error;
        if (!config.save(AppPaths::inputConfigPath(), &error))
            QMessageBox::warning(this, "Xfusion", error);
    }

    if (pause && m_emu)
        m_emu->setPaused(false);
}

void MainWindow::resetGame()
{
    if (!m_emu)
        return;
    m_emu->withCore([this] { m_core.reset(); });
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange) {
        m_windowActive = isActiveWindow();
        if (!m_windowActive)
            m_input.clearKeys();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setPaused(bool paused)
{
    if (!m_emu)
        return;
    m_emu->setPaused(paused);
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

void MainWindow::updateActions()
{
    const bool playing = m_core.hasGame();
    m_closeAction->setEnabled(playing);
    m_pauseAction->setEnabled(playing);
    m_resetAction->setEnabled(playing);
}

void MainWindow::handleKey(QKeyEvent* event, bool pressed)
{
    if (event->isAutoRepeat())
        return;
    m_input.setKeyDown(event->key(), pressed);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        toggleFullscreen();
        return;
    }
    handleKey(event, true);
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    handleKey(event, false);
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    closeGame();
    event->accept();
}
