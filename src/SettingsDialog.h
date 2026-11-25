#pragma once

#include <QDialog>

#include "AppSettings.h"
#include "InputConfig.h"
#include "InputManager.h"

class Achievements;
class AchievementsPage;
class ControllersPage;
class DisplayPage;
class EmulationPage;
class FilesPage;
class GeneralPage;
class QListWidget;
class QStackedWidget;
class ShortcutsPage;
class SoundPage;

// Settings window: a page list on the left and the selected page on the right, with the same pages in the same
// order as snes9xrd. The settings are edited on copies; the caller reads them back only when the dialog is accepted.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Page { General, Display, Sound, Emulation, Controllers, Shortcuts, Files, RetroAchievements };

    SettingsDialog(InputManager& input, const InputConfig& config, const AppSettings& settings, InputGroup initialGroup,
                   Page initialPage, Achievements* achievements, QWidget* parent = nullptr);

    InputConfig inputConfig() const;
    AppSettings appSettings() const;

private:
    void restoreCurrentPageDefaults();
    void showCurrentPageHelp();

    AppSettings m_settings;
    QListWidget* m_pages;
    QStackedWidget* m_stack;
    GeneralPage* m_general;
    DisplayPage* m_display;
    SoundPage* m_sound;
    EmulationPage* m_emulation;
    ControllersPage* m_controllers;
    ShortcutsPage* m_shortcuts;
    FilesPage* m_files;
    AchievementsPage* m_achievementsPage;
};
