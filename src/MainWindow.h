#pragma once

#include <QHash>
#include <QMainWindow>
#include <QTimer>
#include <atomic>
#include <memory>

#include "AudioOutput.h"
#include "EmuThread.h"
#include "LibretroCore.h"
#include "VideoWidget.h"

class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(LibretroCore& core, QWidget* parent = nullptr);

    void openGame(const QString& path);
    void enableStats();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void promptOpenGame();
    void closeGame();
    void pollKeyboard();
    void setPaused(bool paused);
    void resetGame();
    void toggleFullscreen();
    void updateActions();
    void logStats();
    void handleKey(QKeyEvent* event, bool pressed);

    LibretroCore& m_core;
    VideoWidget* m_video;
    // m_emu is declared after m_audio so the thread stops before the audio device it uses is destroyed.
    std::unique_ptr<AudioOutput> m_audio;
    std::unique_ptr<EmuThread> m_emu;
    QTimer m_statsTimer;
    std::atomic<bool> m_windowActive{true};
    QString m_saveRamPath;
    QString m_lastDir;

    QAction* m_closeAction;
    QAction* m_pauseAction;
    QAction* m_resetAction;
};
