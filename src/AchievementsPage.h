#pragma once

#include <QWidget>

#include "AppSettings.h"

class Achievements;
class QCheckBox;
class QLabel;
class QPushButton;
class QSpinBox;

// Settings > Achievements, the last page like in snes9xrd: the account, what to track and what to show.
class AchievementsPage : public QWidget {
    Q_OBJECT
public:
    AchievementsPage(const AppSettings& settings, Achievements* achievements, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    // Everything but the account.
    void restoreDefaults();

private:
    void refreshAccount();

    Achievements* m_achievements;
    QString m_username; // the saved account, as the settings had it
    QString m_token;
    bool m_loggedOut = false; // Logout was pressed: the saved account goes
    QLabel* m_account;
    QPushButton* m_login;
    QPushButton* m_logout;
    QPushButton* m_profile;
    QCheckBox* m_enabled;
    QCheckBox* m_spectator;
    QCheckBox* m_encore;
    QCheckBox* m_unofficial;
    QCheckBox* m_notifications;
    QCheckBox* m_sound;
    QCheckBox* m_leaderboardNotifications;
    QCheckBox* m_leaderboardTrackers;
    QSpinBox* m_duration;
    QCheckBox* m_progress;
    QCheckBox* m_challenge;
    QLabel* m_game;
    QPushButton* m_list;
};
