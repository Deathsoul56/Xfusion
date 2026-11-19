#include "MainWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "AppPaths.h"
#include "SystemInfo.h"

namespace {

// Genesis buttons mapped onto the libretro pad ids PicoDrive translates (B=B, A=Y, C=A, X=L, Y=X, Z=R, Mode=Select).
const QHash<int, unsigned>& keyMap()
{
    static const QHash<int, unsigned> map = {
        {Qt::Key_Up, RETRO_DEVICE_ID_JOYPAD_UP},
        {Qt::Key_Down, RETRO_DEVICE_ID_JOYPAD_DOWN},
        {Qt::Key_Left, RETRO_DEVICE_ID_JOYPAD_LEFT},
        {Qt::Key_Right, RETRO_DEVICE_ID_JOYPAD_RIGHT},
        {Qt::Key_Z, RETRO_DEVICE_ID_JOYPAD_Y},
        {Qt::Key_X, RETRO_DEVICE_ID_JOYPAD_B},
        {Qt::Key_C, RETRO_DEVICE_ID_JOYPAD_A},
        {Qt::Key_A, RETRO_DEVICE_ID_JOYPAD_L},
        {Qt::Key_S, RETRO_DEVICE_ID_JOYPAD_X},
        {Qt::Key_D, RETRO_DEVICE_ID_JOYPAD_R},
        {Qt::Key_Return, RETRO_DEVICE_ID_JOYPAD_START},
        {Qt::Key_Enter, RETRO_DEVICE_ID_JOYPAD_START},
        {Qt::Key_Backspace, RETRO_DEVICE_ID_JOYPAD_SELECT},
    };
    return map;
}

}

MainWindow::MainWindow(LibretroCore& core, QWidget* parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_video(new VideoWidget(this))
{
    setWindowTitle("Xfusion");
    setCentralWidget(m_video);
    setFocusPolicy(Qt::StrongFocus);
    resize(960, 720);

    m_core.setVideoCallback([this](const QImage& frame, double aspect) { m_video->setFrame(frame, aspect); });
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
    m_resetAction = emuMenu->addAction("&Reset", QKeySequence("Ctrl+R"), this, [this] { m_core.reset(); });

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("&Fullscreen", QKeySequence(Qt::Key_F11), this, &MainWindow::toggleFullscreen);

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(2);
    connect(&m_timer, &QTimer::timeout, this, &MainWindow::tick);

    statusBar()->showMessage(QString("Core: %1 - open a game to start").arg(m_core.name()));
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
    if (!m_core.loadGame(path, &error)) {
        QMessageBox::critical(this, "Xfusion", error);
        return;
    }

    m_saveRamPath = saveDir + "/" + QFileInfo(path).completeBaseName() + ".srm";
    m_core.readSaveRam(m_saveRamPath);

    m_audio = std::make_unique<AudioOutput>(m_core.avInfo().sampleRate);
    m_pauseAction->setChecked(false);
    m_paused = false;
    m_accumNs = 0;
    m_clock.start();
    m_lastNs = 0;
    m_timer.start();

    setWindowTitle(QString("Xfusion - %1").arg(QFileInfo(path).completeBaseName()));
    statusBar()->showMessage(QString("%1 | %2 | saves: %3").arg(m_core.name(), systemDef(*system).folder, saveDir));
    updateActions();
    setFocus();
}

void MainWindow::closeGame()
{
    if (!m_core.hasGame())
        return;

    m_timer.stop();
    if (!m_core.writeSaveRam(m_saveRamPath))
        QMessageBox::warning(this, "Xfusion", QString("Could not write save file: %1").arg(m_saveRamPath));
    m_core.unloadGame();
    m_audio.reset();
    m_video->clear();
    setWindowTitle("Xfusion");
    statusBar()->showMessage("Game closed");
    updateActions();
}

// Polling the physical key state avoids losing press/release events after dialogs or focus changes.
void MainWindow::pollKeyboard()
{
#ifdef Q_OS_WIN
    static const struct { int vk; unsigned id; } binds[] = {
        {VK_UP, RETRO_DEVICE_ID_JOYPAD_UP},       {VK_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN},
        {VK_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT},   {VK_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT},
        {'Z', RETRO_DEVICE_ID_JOYPAD_Y},          {'X', RETRO_DEVICE_ID_JOYPAD_B},
        {'C', RETRO_DEVICE_ID_JOYPAD_A},          {'A', RETRO_DEVICE_ID_JOYPAD_L},
        {'S', RETRO_DEVICE_ID_JOYPAD_X},          {'D', RETRO_DEVICE_ID_JOYPAD_R},
        {VK_RETURN, RETRO_DEVICE_ID_JOYPAD_START}, {VK_BACK, RETRO_DEVICE_ID_JOYPAD_SELECT},
    };
    const bool active = isActiveWindow();
    for (const auto& b : binds)
        m_core.setButton(b.id, active && (GetAsyncKeyState(b.vk) & 0x8000));
#endif
}

void MainWindow::tick()
{
    pollKeyboard();
    const qint64 now = m_clock.nsecsElapsed();
    m_accumNs += now - m_lastNs;
    m_lastNs = now;

    const qint64 frameNs = qint64(1e9 / m_core.avInfo().fps);
    int frames = 0;
    while (m_accumNs >= frameNs && frames < 4) {
        m_core.run();
        m_accumNs -= frameNs;
        ++frames;
    }
    if (m_accumNs > frameNs * 4)
        m_accumNs = 0;
}

void MainWindow::setPaused(bool paused)
{
    if (!m_core.hasGame())
        return;
    m_paused = paused;
    if (paused) {
        m_timer.stop();
    } else {
        m_lastNs = m_clock.nsecsElapsed();
        m_accumNs = 0;
        m_timer.start();
    }
    if (m_audio)
        m_audio->setPaused(paused);
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
    const auto it = keyMap().constFind(event->key());
    if (it != keyMap().constEnd())
        m_core.setButton(it.value(), pressed);
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
