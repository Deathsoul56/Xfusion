#include "AchievementsDialogs.h"

#include <QDialogButtonBox>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

AchievementsLoginDialog::AchievementsLoginDialog(Achievements* achievements, QWidget* parent)
    : QDialog(parent)
    , m_achievements(achievements)
    , m_username(new QLineEdit(this))
    , m_password(new QLineEdit(this))
    , m_status(new QLabel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle("RetroAchievements Login");
    m_password->setEchoMode(QLineEdit::Password);
    m_status->setWordWrap(true);
    m_buttons->button(QDialogButtonBox::Ok)->setText("Login");

    auto* form = new QFormLayout;
    form->addRow("Username:", m_username);
    form->addRow("Password:", m_password);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Sign in with your RetroAchievements account. The password is not kept: only the session token the server gives back is saved, encrypted for this computer.", this));
    static_cast<QLabel*>(layout->itemAt(0)->widget())->setWordWrap(true);
    layout->addLayout(form);
    layout->addWidget(m_status);
    layout->addWidget(m_buttons);
    resize(420, 220);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &AchievementsLoginDialog::attempt);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_achievements, &Achievements::accountChanged, this, &AchievementsLoginDialog::finished);
}

void AchievementsLoginDialog::attempt()
{
    if (m_username->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_status->setText("Enter the user name and the password.");
        return;
    }
    m_status->setText("Signing in...");
    m_buttons->setEnabled(false);
    m_achievements->loginWithPassword(m_username->text().trimmed(), m_password->text());
}

void AchievementsLoginDialog::finished()
{
    if (m_achievements->loginPending())
        return;
    m_buttons->setEnabled(true);
    if (m_achievements->user().loggedIn) {
        m_password->clear();
        accept();
    } else {
        m_status->setText(m_achievements->lastError().isEmpty() ? QString("Could not sign in.") : m_achievements->lastError());
    }
}

AchievementsListDialog::AchievementsListDialog(Achievements* achievements, QWidget* parent)
    : QDialog(parent)
{
    const Achievements::GameSummary summary = achievements->summary();
    setWindowTitle(summary.loaded ? QString("Achievements - %1").arg(summary.title) : QString("Achievements"));
    resize(680, 520);
    auto* layout = new QVBoxLayout(this);
    if (!summary.loaded) {
        layout->addWidget(new QLabel("No game with achievements is loaded.", this));
    } else {
        layout->addWidget(new QLabel(QString("%1 of %2 achievements unlocked, %3 of %4 points.").arg(summary.unlocked).arg(summary.achievements).arg(summary.pointsUnlocked).arg(summary.points), this));
        const QList<Achievements::Entry> entries = achievements->entries();
        auto* table = new QTableWidget(int(entries.size()), 4, this);
        table->setHorizontalHeaderLabels({"", "Title", "Description", "Points"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        for (int row = 0; row < entries.size(); ++row) {
            const Achievements::Entry& e = entries[row];
            table->setItem(row, 0, new QTableWidgetItem(e.unlocked ? QString::fromUtf8("\xE2\x9C\x93") : QString()));
            auto* title = new QTableWidgetItem(e.title);
            title->setData(Qt::UserRole, e.badgeUrl);
            table->setItem(row, 1, title);
            table->setItem(row, 2, new QTableWidgetItem(e.description));
            table->setItem(row, 3, new QTableWidgetItem(QString::number(e.points)));
        }
        table->setIconSize(QSize(40, 40));
        table->verticalHeader()->setDefaultSectionSize(48);
        // The badges come from the server one by one and are put in as they arrive.
        connect(achievements, &Achievements::badgeReady, table, [table](const QString& url, const QImage& image) {
            if (image.isNull())
                return;
            for (int row = 0; row < table->rowCount(); ++row)
                if (QTableWidgetItem* item = table->item(row, 1); item && item->data(Qt::UserRole).toString() == url)
                    item->setIcon(QIcon(QPixmap::fromImage(image)));
            table->resizeColumnToContents(1);
        });
        for (const Achievements::Entry& e : entries)
            if (!e.badgeUrl.isEmpty())
                achievements->requestBadge(e.badgeUrl);
        table->resizeColumnToContents(0);
        table->resizeColumnToContents(1);
        layout->addWidget(table, 1);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
