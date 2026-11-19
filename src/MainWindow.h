#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>
#include <QTimer>
#include <memory>

#include "AudioOutput.h"
#include "LibretroCore.h"
#include "VideoWidget.h"

class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(LibretroCore& core, QWidget* parent = nullptr);

    void openGame(const QString& path);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void promptOpenGame();
    void closeGame();
    void tick();
    void pollKeyboard();
    void setPaused(bool paused);
    void toggleFullscreen();
    void updateActions();
    void handleKey(QKeyEvent* event, bool pressed);

    LibretroCore& m_core;
    VideoWidget* m_video;
    std::unique_ptr<AudioOutput> m_audio;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;
    qint64 m_accumNs = 0;
    bool m_paused = false;
    QString m_saveRamPath;
    QString m_lastDir;

    QAction* m_closeAction;
    QAction* m_pauseAction;
    QAction* m_resetAction;
};
