#pragma once

#include <QDialog>

#include "Achievements.h"

class QLabel;
class QLineEdit;
class QDialogButtonBox;

// Asks for the name and password of a RetroAchievements account and signs in. The password is only handed to the
// server through rcheevos; what stays on disk is the token the server gives back.
class AchievementsLoginDialog : public QDialog {
    Q_OBJECT
public:
    AchievementsLoginDialog(Achievements* achievements, QWidget* parent = nullptr);

private:
    void attempt();
    void finished();

    Achievements* m_achievements;
    QLineEdit* m_username;
    QLineEdit* m_password;
    QLabel* m_status;
    QDialogButtonBox* m_buttons;
};

// The achievements of the game that is running, locked and unlocked.
class AchievementsListDialog : public QDialog {
    Q_OBJECT
public:
    AchievementsListDialog(Achievements* achievements, QWidget* parent = nullptr);
};
