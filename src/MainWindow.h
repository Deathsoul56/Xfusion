#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>
#include <QTimer>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>

#include "AppSettings.h"
#include "AudioOutput.h"
#include "EmuThread.h"
#include "GameSaves.h"
#include "InputManager.h"
#include "LibretroCore.h"
#include "VideoWidget.h"

class QAction;
class QActionGroup;
class LibraryPage;
class QLabel;
class QMenu;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(LibretroCore& core, QWidget* parent = nullptr);
    ~MainWindow() override;

    void openGame(const QString& path);
    void enableStats();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    static constexpr int kPlayers = 2;

    // Menus and shortcuts
    void createMenus();
    void createFileMenu();
    void createEmulationMenu();
    void createViewMenu();
    void createOptionsMenu();
    void refreshMenuHints();
    void populateRecentMenu();
    void populatePositionMenu(QMenu* menu, bool saving);
    void trigger(ShortcutAction action);
    void setHold(ShortcutAction action, bool down);
    void pollShortcutJoysticks();
    void rebuildJoystickShortcuts();

    // Games
    void promptOpenGame();
    bool closeGame();
    bool prepareDisc(const QString& path);
    void resetGame(bool powerCycle);
    void showRomInfo();
    void takeScreenshot();
    void saveSramNow();
    void loadSaveRam();
    void flushSaveRam(bool closing);

    // States
    bool stateOperationsAvailable() const;
    QString slotDescription(int slot) const;
    void setCurrentSlot(int slot);
    // `confirm` asks before overwriting or loading when the setting is on; the quick slots never ask.
    void saveStateToSlot(int slot, bool confirm = false);
    void loadStateFromSlot(int slot, bool confirm = false);
    void saveStateToFile(const QString& path, int confirmSlot = -1);
    void loadStateFromFile(const QString& path);
    void loadUndoState();
    void loadOopsState();
    void maybeSaveOops();
    bool captureState(QByteArray* state, QString* error);
    bool writeState(const QString& path, const QByteArray& state);
    bool applyState(const QByteArray& state, const QString& what, bool makeUndo);
    void showStatePreview(bool saving);

    // Settings and state of the session
    void openSettings(int page);
    void applySettings();
    void commitSettings(const QString& message);
    void adjustVolume(int delta);
    void toggleMute();
    void changeSpeed(int direction);
    void changeFrameSkip(int delta);
    SpeedConfig speedConfig() const;
    void updateSpeed();
    void updatePause();
    // The library counts the time a game runs, not the time it is paused. Sessions are added when the game closes
    // and every minute, so a crash loses at most a minute.
    void trackPlayTime(bool running);
    void commitPlayTime();
    void setManualPause(bool paused);
    void updateAudioMode();
    void toggleFullscreen();
    void resizeToMultiple(int multiple);
    void updateActions();
    void showMessage(const QString& text);
    void pollInput();
    void logStats();
    void updateOverlays();
    void loadSession();
    void saveSession() const;
    void addRecent(const QString& path);

    LibretroCore& m_core;
    VideoWidget* m_video;
    LibraryPage* m_library;
    QStackedWidget* m_central;
    // m_emu is declared after m_audio so the thread stops before the audio device it uses is destroyed.
    std::unique_ptr<AudioOutput> m_audio;
    std::unique_ptr<EmuThread> m_emu;
    QTimer m_statsTimer;
    QTimer m_overlayTimer;
    QTimer m_joystickTimer;
    InputManager m_input;
    AppSettings m_settings;

    // The game
    QString m_gamePath;
    std::unique_ptr<GameSaves> m_saves;
    std::optional<System> m_gameSystem;
    std::atomic<InputGroup> m_inputGroup{InputGroup::Genesis};
    std::atomic<bool> m_windowActive{true};
    QByteArray m_lastSaveRam; // what the save file holds, to skip writing when nothing changed
    bool m_saveRamLocked = false; // the save file could not be read as it is, so it must not be overwritten
    bool m_saveWriteFailing = false;
    QTimer m_saveTimer;
    bool m_statesSupported = false;
    int m_currentSlot = 0;
    qint64 m_lastStateActivityMs = 0;

    QTimer m_playTimer;
    QElapsedTimer m_playClock;
    qint64 m_playPendingMs = 0;
    bool m_playRunning = false;

    // Pausing: any reason keeps the game paused
    bool m_manualPause = false;
    bool m_focusPause = false;
    bool m_minimizedPause = false;
    int m_dialogPause = 0;

    // Fast-forward, rewind and the audio they change
    bool m_fastForwardHeld = false;
    bool m_fastForwardToggled = false;
    std::atomic<bool> m_muteFastForward{false};
    std::atomic<bool> m_muteStepping{false};
    std::atomic<bool> m_backgroundPads{false};

    // Shortcuts
    std::map<int, std::vector<ShortcutAction>> m_heldByKey;
    std::array<bool, kShortcutCount> m_joyDown{};
    std::vector<ShortcutAction> m_joystickActions;

    // Input read by the emulation thread
    int m_turboFrame = 0;
    std::atomic<unsigned> m_pressedMask{0};

    // Session
    QStringList m_recent;
    QString m_lastDir;
    QString m_romDir;

    std::map<ShortcutAction, QAction*> m_actions;
    std::map<ShortcutAction, QString> m_actionTexts;
    QAction* m_closeAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_pauseUnfocusedAction = nullptr;
    QAction* m_romInfoAction = nullptr;
    QMenu* m_recentMenu = nullptr;
    QMenu* m_saveMenu = nullptr;
    QMenu* m_loadMenu = nullptr;
    std::vector<QAction*> m_gameActions; // enabled only while a game is open
    std::vector<QAction*> m_stateActions; // enabled only while the core supports states
};
