#include "CoresDialog.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "AppPaths.h"
#include "CoreInstaller.h"
#include "CoreManifest.h"
#include "CoreProbe.h"
#include "CoreProfile.h"
#include "CoreRules.h"

CoresDialog::CoresDialog(QWidget* parent)
    : QDialog(parent)
    , m_installer(new CoreInstaller(this))
    , m_message(new QLabel(this))
{
    setWindowTitle("Manage Cores");
    resize(640, 560);
    auto* layout = new QVBoxLayout(this);

    for (const CoreInfo& core : allCores()) {
        Panel panel;
        panel.coreId = core.id;
        auto* box = new QGroupBox(core.name, this);
        auto* boxLayout = new QVBoxLayout(box);
        panel.status = new QLabel(box);
        panel.status->setWordWrap(true);
        panel.versions = new QListWidget(box);
        panel.versions->setMaximumHeight(90);
        panel.use = new QPushButton("Use Selected", box);
        panel.use->setToolTip("Makes the selected version the one games use from now on. It applies the next time a game opens.");
        panel.remove = new QPushButton("Remove Selected", box);
        panel.remove->setToolTip("Deletes an installed version that is not the active one.");
        panel.check = new QPushButton("Check for Update", box);
        panel.check->setToolTip("Asks the libretro buildbot for the date of the latest build, without downloading it.");
        panel.install = new QPushButton("Install Latest", box);
        panel.install->setToolTip("Downloads the latest build from the libretro buildbot and makes it the active version. The versions you had stay installed.");
        auto* buttons = new QHBoxLayout;
        buttons->addWidget(panel.use);
        buttons->addWidget(panel.remove);
        buttons->addStretch(1);
        buttons->addWidget(panel.check);
        buttons->addWidget(panel.install);
        boxLayout->addWidget(panel.status);
        boxLayout->addWidget(panel.versions);
        boxLayout->addLayout(buttons);
        layout->addWidget(box);
        m_panels.append(panel);
    }
    m_message->setWordWrap(true);
    layout->addWidget(m_message);
    layout->addStretch(1);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(close);

    for (const Panel& panel : m_panels) {
        connect(panel.use, &QPushButton::clicked, this, [this, panel] { useSelected(panel); });
        connect(panel.remove, &QPushButton::clicked, this, [this, panel] { removeSelected(panel); });
        connect(panel.check, &QPushButton::clicked, this, [this, panel] {
            setBusy(true);
            m_installer->checkForUpdate(panel.coreId);
        });
        connect(panel.install, &QPushButton::clicked, this, [this, panel] {
            setBusy(true);
            m_installer->install(panel.coreId);
        });
    }
    connect(m_installer, &CoreInstaller::progress, this, &CoresDialog::say);
    connect(m_installer, &CoreInstaller::checked, this, [this](const QString& coreId, const QString& serverDate, const QString& error) {
        setBusy(false);
        if (!error.isEmpty()) {
            say("Could not reach the server: " + error);
            return;
        }
        m_serverDates[coreId] = serverDate;
        refresh();
        say(QString("Latest build on the server: %1").arg(serverDate.isEmpty() ? QString("date unknown") : QDateTime::fromString(serverDate, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd hh:mm")));
    });
    connect(m_installer, &CoreInstaller::finished, this, [this](const QString& coreId, const QString& version, bool alreadyThere, const QString& error) {
        setBusy(false);
        refresh();
        if (!error.isEmpty()) {
            say("Could not install the core: " + error);
            return;
        }
        Q_UNUSED(coreId)
        say(alreadyThere ? QString("That build was installed already: %1 is the active version again.").arg(version)
                         : QString("Installed %1 and made it the active version. It applies the next time a game opens.").arg(version));
    });
    refresh();
}

void CoresDialog::say(const QString& text)
{
    m_message->setText(text);
}

void CoresDialog::setBusy(bool busy)
{
    for (const Panel& panel : m_panels)
        for (QPushButton* button : {panel.use, panel.remove, panel.check, panel.install})
            button->setEnabled(!busy);
}

void CoresDialog::refresh()
{
    QString error;
    const CoreManifest manifest = CoreManifest::load(AppPaths::coresDir(), &error);
    if (!error.isEmpty())
        say(error);

    for (const Panel& panel : m_panels) {
        panel.versions->clear();
        const QString activeVersion = manifest.activeVersion(panel.coreId);
        const InstalledCore* active = manifest.active(panel.coreId);

        // The library that sits in the cores folder, when there is one: it plays when no version is active.
        const QString local = AppPaths::coresDir() + "/" + coreProfileById(panel.coreId).libraryFileName();
        QString localVersion;
        if (QFileInfo::exists(local)) {
            QString name, version, probeError;
            localVersion = probeCore(local, &name, &version, &probeError) ? version : QString("unreadable");
            auto* item = new QListWidgetItem(QString("Local file: %1%2").arg(localVersion, activeVersion.isEmpty() ? "  (active)" : ""), panel.versions);
            item->setData(Qt::UserRole, QString());
            item->setData(Qt::UserRole + 1, true);
            if (activeVersion.isEmpty())
                panel.versions->setCurrentItem(item);
        }
        for (const InstalledCore& core : manifest.versions(panel.coreId)) {
            const QString when = QDateTime::fromString(core.installedAt, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd");
            auto* item = new QListWidgetItem(QString("%1  (downloaded %2)%3").arg(core.version, when, core.version == activeVersion ? "  (active)" : ""), panel.versions);
            item->setData(Qt::UserRole, core.version);
            if (core.version == activeVersion)
                panel.versions->setCurrentItem(item);
        }

        QString status = active ? QString("Active: %1").arg(active->version)
                                : localVersion.isEmpty() ? QString("Not installed. Install the latest build, or copy %1 into %2.").arg(coreProfileById(panel.coreId).libraryFileName(), QDir::toNativeSeparators(AppPaths::coresDir()))
                                                         : QString("Active: the local file (%1)").arg(localVersion);
        const QString server = m_serverDates.value(panel.coreId);
        if (!server.isEmpty()) {
            const bool newer = !active || active->serverDate.isEmpty() || QDateTime::fromString(server, Qt::ISODate) > QDateTime::fromString(active->serverDate, Qt::ISODate);
            status += newer ? (active ? "\nA newer build is available." : "\nThe server has a build; Install Latest keeps track of it.") : "\nUp to date.";
        }
        panel.status->setText(status);
    }
}

void CoresDialog::useSelected(const Panel& panel)
{
    const QListWidgetItem* item = panel.versions->currentItem();
    if (!item)
        return;
    CoreManifest manifest = CoreManifest::load(AppPaths::coresDir());
    const QString version = item->data(Qt::UserRole).toString();
    if (version.isEmpty())
        manifest.clearActive(panel.coreId);
    else if (!manifest.setActive(panel.coreId, version))
        return;
    QString error;
    if (!manifest.save(&error)) {
        say(error);
        return;
    }
    refresh();
    say("Done. It applies the next time a game opens.");
}

void CoresDialog::removeSelected(const Panel& panel)
{
    const QListWidgetItem* item = panel.versions->currentItem();
    if (!item)
        return;
    const QString version = item->data(Qt::UserRole).toString();
    if (version.isEmpty()) {
        say("The local file is not managed here; delete it from the cores folder if you do not want it.");
        return;
    }
    CoreManifest manifest = CoreManifest::load(AppPaths::coresDir());
    const InstalledCore* core = manifest.find(panel.coreId, version);
    if (!core)
        return;
    if (manifest.activeVersion(panel.coreId) == version) {
        say("That version is the active one. Make another one active first.");
        return;
    }
    if (QMessageBox::question(this, "Remove Core", QString("Delete %1?").arg(version)) != QMessageBox::Yes)
        return;
    const QString folder = QFileInfo(QDir(AppPaths::coresDir()).absoluteFilePath(core->path)).absolutePath();
    if (!QDir(folder).removeRecursively()) {
        say("The files could not be deleted. If a game is open with this core, close it and try again.");
        return;
    }
    manifest.remove(panel.coreId, version);
    QString error;
    if (!manifest.save(&error))
        say(error);
    else
        say(QString("Removed %1.").arg(version));
    refresh();
}
