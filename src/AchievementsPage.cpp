#include "AchievementsPage.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "Achievements.h"
#include "AchievementsDialogs.h"
#include "AchievementsToken.h"
#include "FormAlign.h"

AchievementsPage::AchievementsPage(const AppSettings& settings, Achievements* achievements, QWidget* parent)
    : QWidget(parent)
    , m_achievements(achievements)
    , m_account(new QLabel(this))
    , m_login(new QPushButton("Login...", this))
    , m_logout(new QPushButton("Logout", this))
    , m_profile(new QPushButton("View Profile...", this))
    , m_enabled(new QCheckBox("Enable Achievements", this))
    , m_spectator(new QCheckBox("Enable Spectator Mode", this))
    , m_encore(new QCheckBox("Enable Encore Mode", this))
    , m_unofficial(new QCheckBox("Track Unofficial Achievements", this))
    , m_notifications(new QCheckBox("Show Achievement Unlock Notifications", this))
    , m_sound(new QCheckBox("Play a Sound When an Achievement Unlocks", this))
    , m_leaderboardNotifications(new QCheckBox("Show Leaderboard Notifications", this))
    , m_leaderboardTrackers(new QCheckBox("Show Leaderboard Trackers", this))
    , m_duration(new QSpinBox(this))
    , m_progress(new QCheckBox("Show Achievement Progress Indicators", this))
    , m_challenge(new QCheckBox("Show Challenge Indicators", this))
    , m_game(new QLabel(this))
    , m_list(new QPushButton("View Achievement List...", this))
{
    m_enabled->setToolTip("Tracks the achievements of the running game with RetroAchievements. When off, nothing is read, sent or tracked.");
    m_spectator->setToolTip("Unlocks and leaderboard scores are tracked here but never sent to the server.");
    m_encore->setToolTip("An achievement that was already unlocked can be earned again. Takes effect the next time a game is loaded.");
    m_unofficial->setToolTip("Also the achievements that are still being made. Takes effect the next time a game is loaded.");
    m_notifications->setToolTip("Shows a message when an achievement unlocks or the game is completed.");
    m_sound->setToolTip("Plays a short chime when an achievement unlocks.");
    m_leaderboardNotifications->setToolTip("Shows a message when a leaderboard attempt starts, fails or is submitted.");
    m_leaderboardTrackers->setToolTip("Shows the live value of the leaderboard attempt in progress.");
    m_duration->setRange(1, 30);
    m_duration->setSuffix(" seconds");
    m_duration->setToolTip("How long the notices stay on screen.");
    m_progress->setToolTip("Shows the progress (for example 4/10) of achievements that count something.");
    m_challenge->setToolTip("Shows a message when an achievement that has to be done in one go starts.");

    auto* account = new QGroupBox("Account", this);
    auto* accountLayout = new QVBoxLayout(account);
    m_account->setWordWrap(true);
    accountLayout->addWidget(m_account);
    auto* accountButtons = new QHBoxLayout;
    accountButtons->addWidget(m_login);
    accountButtons->addWidget(m_logout);
    accountButtons->addWidget(m_profile);
    accountButtons->addStretch(1);
    accountLayout->addLayout(accountButtons);

    auto* settingsBox = new QGroupBox("Settings", this);
    auto* settingsLayout = new QVBoxLayout(settingsBox);
    for (QCheckBox* box : {m_enabled, m_spectator, m_encore, m_unofficial})
        settingsLayout->addWidget(box);
    auto* hardcore = new QLabel("Hardcore Mode is not offered. RetroAchievements only counts it from approved emulators, and Xfusion is not one yet; achievements are earned in casual mode. Leaderboards only work in hardcore mode, so they are not available. Games with a patch applied have no achievements.", settingsBox);
    hardcore->setWordWrap(true);
    hardcore->setEnabled(false);
    settingsLayout->addWidget(hardcore);

    auto* notificationsBox = new QGroupBox("Notifications", this);
    auto* notificationsLayout = new QFormLayout(notificationsBox);
    notificationsLayout->addRow(m_notifications);
    notificationsLayout->addRow(m_sound);
    // Leaderboards only count in hardcore mode, which is not offered: their settings stay saved but are not shown.
    m_leaderboardNotifications->hide();
    m_leaderboardTrackers->hide();
    notificationsLayout->addRow("Notification Duration:", m_duration);

    auto* progressBox = new QGroupBox("Progress Tracking", this);
    auto* progressLayout = new QVBoxLayout(progressBox);
    progressLayout->addWidget(m_progress);
    progressLayout->addWidget(m_challenge);

    auto* gameBox = new QGroupBox("Current Game", this);
    auto* gameLayout = new QVBoxLayout(gameBox);
    gameLayout->addWidget(m_game);
    gameLayout->addWidget(m_list, 0, Qt::AlignLeft);

    auto* about = new QLabel("Xfusion uses RetroAchievements as an achievement database and to track progress. To use achievements, sign up for an account at <a href=\"https://retroachievements.org/\">retroachievements.org</a>.", this);
    about->setWordWrap(true);
    about->setOpenExternalLinks(true);
    about->setEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(account);
    layout->addWidget(settingsBox);
    layout->addWidget(notificationsBox);
    layout->addWidget(progressBox);
    layout->addWidget(gameBox);
    layout->addWidget(about);
    layout->addStretch(1);
    alignFormLabels(this);

    connect(m_login, &QPushButton::clicked, this, [this] {
        AchievementsLoginDialog(m_achievements, this).exec();
        m_loggedOut = false;
        refreshAccount();
    });
    connect(m_logout, &QPushButton::clicked, this, [this] {
        m_achievements->logout();
        m_loggedOut = true;
        refreshAccount();
    });
    connect(m_profile, &QPushButton::clicked, this, [this] {
        const QString name = m_achievements->user().username;
        if (!name.isEmpty())
            QDesktopServices::openUrl(QUrl("https://retroachievements.org/user/" + name));
    });
    connect(m_list, &QPushButton::clicked, this, [this] { AchievementsListDialog(m_achievements, this).exec(); });
    connect(m_achievements, &Achievements::accountChanged, this, &AchievementsPage::refreshAccount);
    connect(m_achievements, &Achievements::gameChanged, this, &AchievementsPage::refreshAccount);

    setValues(settings);
}

void AchievementsPage::refreshAccount()
{
    const Achievements::User user = m_achievements->user();
    if (user.loggedIn)
        m_account->setText(QString("Signed in as %1 (%2 points).").arg(user.displayName.isEmpty() ? user.username : user.displayName).arg(user.score));
    else if (m_achievements->loginPending())
        m_account->setText("Signing in...");
    else if (!m_username.isEmpty() && !m_loggedOut)
        m_account->setText(QString("Saved account: %1. %2").arg(m_username, m_achievements->lastError().isEmpty() ? QString("Not signed in yet.") : m_achievements->lastError()));
    else
        m_account->setText("Not logged in.");
    m_login->setEnabled(!user.loggedIn && !m_achievements->loginPending());
    m_logout->setEnabled(user.loggedIn || (!m_username.isEmpty() && !m_loggedOut));
    m_profile->setEnabled(user.loggedIn);

    const Achievements::GameSummary game = m_achievements->summary();
    m_game->setText(game.loaded ? QString("%1\n%2 of %3 achievements unlocked.").arg(game.title).arg(game.unlocked).arg(game.achievements) : QString("No game with achievements is loaded."));
    m_list->setEnabled(game.loaded);
}

void AchievementsPage::setValues(const AppSettings& settings)
{
    const AchievementsSettings& a = settings.achievements;
    m_username = a.username;
    m_token = a.token;
    m_loggedOut = false;
    m_enabled->setChecked(a.enabled);
    m_spectator->setChecked(a.spectator);
    m_encore->setChecked(a.encore);
    m_unofficial->setChecked(a.unofficial);
    m_notifications->setChecked(a.notifications);
    m_sound->setChecked(a.unlockSound);
    m_leaderboardNotifications->setChecked(a.leaderboardNotifications);
    m_leaderboardTrackers->setChecked(a.leaderboardTrackers);
    m_duration->setValue(a.durationSeconds);
    m_progress->setChecked(a.progressIndicators);
    m_challenge->setChecked(a.challengeIndicators);
    refreshAccount();
}

void AchievementsPage::applyTo(AppSettings& settings) const
{
    AchievementsSettings& a = settings.achievements;
    a.enabled = m_enabled->isChecked();
    a.spectator = m_spectator->isChecked();
    a.encore = m_encore->isChecked();
    a.unofficial = m_unofficial->isChecked();
    a.notifications = m_notifications->isChecked();
    a.unlockSound = m_sound->isChecked();
    a.leaderboardNotifications = m_leaderboardNotifications->isChecked();
    a.leaderboardTrackers = m_leaderboardTrackers->isChecked();
    a.durationSeconds = m_duration->value();
    a.progressIndicators = m_progress->isChecked();
    a.challengeIndicators = m_challenge->isChecked();

    // The account: the one that is signed in now, nothing after Logout, and otherwise what was saved (an attempt
    // that failed for lack of a connection does not lose the saved account).
    const Achievements::User user = m_achievements->user();
    if (user.loggedIn) {
        a.username = user.username;
        a.token = encryptToken(user.username, user.token);
    } else if (m_loggedOut) {
        a.username.clear();
        a.token.clear();
    } else {
        a.username = m_username;
        a.token = m_token;
    }
}

void AchievementsPage::restoreDefaults()
{
    AppSettings defaults;
    defaults.achievements.username = m_username;
    defaults.achievements.token = m_token;
    setValues(defaults);
}
