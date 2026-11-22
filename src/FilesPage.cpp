#include "FilesPage.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QListWidget>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "AppPaths.h"
#include "Bios.h"

FilesPage::FilesPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
{
    m_rows = {{{"Save RAM (SRAM)", "saves", true}, {"Save states", "states", true}, {"Screenshots", "screenshots", true}, {"BIOS", "system", false}, {"Patches", "patches", true}}};

    auto* group = new QGroupBox("Data Locations", this);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(2, 1);
    int rowIndex = 0;
    for (Row& row : m_rows) {
        row.location = new QComboBox(group);
        row.location->addItem("Data Folder", static_cast<int>(FileLocation::DataFolder));
        if (row.allowRomFolder)
            row.location->addItem("Game Folder", static_cast<int>(FileLocation::RomFolder));
        row.location->addItem("Custom Folder", static_cast<int>(FileLocation::Custom));
        row.path = new QLineEdit(group);
        row.path->setReadOnly(true);
        row.button = new QPushButton(group);
        grid->addWidget(new QLabel(row.label, group), rowIndex, 0);
        grid->addWidget(row.location, rowIndex, 1);
        grid->addWidget(row.path, rowIndex, 2);
        grid->addWidget(row.button, rowIndex, 3);
        connect(row.location, &QComboBox::currentIndexChanged, this, [this, &row] { refreshRow(row); });
        connect(row.button, &QPushButton::clicked, this, [this, &row] { buttonClicked(row); });
        ++rowIndex;
    }

    // Sega CD games need a BIOS for the region of the disc; dumps come under any name and are recognised by content.
    m_biosStatus = new QLabel(group);
    auto* findBios = new QPushButton("Find BIOS Files...", group);
    findBios->setToolTip("Looks in a folder for Sega CD BIOS dumps, whatever they are called, and copies the ones that are missing here "
                         "as bios_CD_J.bin, bios_CD_E.bin and bios_CD_U.bin. Nothing already there is replaced.");
    connect(findBios, &QPushButton::clicked, this, &FilesPage::findBios);
    grid->addWidget(new QLabel("Sega CD BIOS", group), rowIndex, 0);
    grid->addWidget(m_biosStatus, rowIndex, 1, 1, 2);
    grid->addWidget(findBios, rowIndex, 3);

    auto* library = new QGroupBox("Library Folders", this);
    m_libraryFolders = new QListWidget(library);
    m_libraryFolders->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_libraryFolders->setToolTip("The folders (and their subfolders) searched for games on the Library page. Games are only read, never changed.");
    auto* addFolder = new QPushButton("Add Folder...", library);
    auto* removeFolder = new QPushButton("Remove Selected", library);
    connect(addFolder, &QPushButton::clicked, this, &FilesPage::addLibraryFolder);
    connect(removeFolder, &QPushButton::clicked, this, [this] { qDeleteAll(m_libraryFolders->selectedItems()); });
    auto* libraryButtons = new QHBoxLayout;
    libraryButtons->addWidget(addFolder);
    libraryButtons->addWidget(removeFolder);
    libraryButtons->addStretch(1);
    auto* libraryLayout = new QVBoxLayout(library);
    libraryLayout->addWidget(m_libraryFolders);
    libraryLayout->addLayout(libraryButtons);

    auto* hint = new QLabel("Saves, states and screenshots keep one folder per system, so games with the same name on different systems never "
                            "overwrite each other. Data Folder is next to the program (or XFUSION_DATA_DIR). Game Folder puts the files "
                            "beside each game instead. Changes apply the next time a game opens.\n\nPatches: a file with the name of the game and the "
                            "extension .bps, .ups or .ips, in the Patches folder (directly or in the folder of its system), is applied when the "
                            "game opens. The game file is not changed, and a patched game has its own saves and states.",
                            this);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->addWidget(hint);
    layout->addWidget(library, 1);

    setValues(settings);
}

void FilesPage::setValues(const AppSettings& s)
{
    const FolderSetting* folders[] = {&s.sramFolder, &s.stateFolder, &s.screenshotFolder, &s.biosFolder, &s.patchFolder};
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        Row& row = m_rows[i];
        row.custom = folders[i]->custom;
        int index = row.location->findData(static_cast<int>(folders[i]->location));
        row.location->setCurrentIndex(std::max(0, index));
        refreshRow(row);
    }
    refreshBiosStatus();
    m_libraryFolders->clear();
    m_libraryFolders->addItems(s.libraryFolders);
}

void FilesPage::applyTo(AppSettings& s) const
{
    FolderSetting* folders[] = {&s.sramFolder, &s.stateFolder, &s.screenshotFolder, &s.biosFolder, &s.patchFolder};
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        const Row& row = m_rows[i];
        folders[i]->location = static_cast<FileLocation>(row.location->currentData().toInt());
        folders[i]->custom = row.custom;
    }
    s.libraryFolders.clear();
    for (int i = 0; i < m_libraryFolders->count(); ++i)
        s.libraryFolders << m_libraryFolders->item(i)->text();
}

QString FilesPage::biosFolder() const
{
    const Row& row = m_rows[3];
    return static_cast<FileLocation>(row.location->currentData().toInt()) == FileLocation::Custom ? row.custom : AppPaths::dataRoot() + "/system";
}

void FilesPage::refreshBiosStatus()
{
    const BiosStatus status = checkBios(biosFolder());
    const auto mark = [&](BiosRegion region) { return QString("%1 %2").arg(biosRegionName(region), status.has(region) ? "\u2713" : "\u2717"); };
    m_biosStatus->setText(QString("%1   %2   %3").arg(mark(BiosRegion::Japan), mark(BiosRegion::Europe), mark(BiosRegion::USA)));
}

void FilesPage::findBios()
{
    const QString folder = QFileDialog::getExistingDirectory(this, "Folder with the BIOS dumps");
    if (folder.isEmpty())
        return;
    QString error;
    const QStringList installed = installBios(folder, biosFolder(), &error);
    refreshBiosStatus();
    if (!error.isEmpty())
        QMessageBox::warning(this, "Xfusion", error);
    else if (installed.isEmpty())
        QMessageBox::information(this, "Xfusion", "No BIOS that is still missing was found in that folder.");
    else
        QMessageBox::information(this, "Xfusion", "Installed:\n" + installed.join("\n"));
}

void FilesPage::addLibraryFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, "Choose a game folder");
    if (folder.isEmpty() || !m_libraryFolders->findItems(folder, Qt::MatchFixedString).isEmpty())
        return;
    m_libraryFolders->addItem(folder);
}

void FilesPage::refreshRow(Row& row)
{
    switch (static_cast<FileLocation>(row.location->currentData().toInt())) {
    case FileLocation::DataFolder:
        row.path->setText(QDir::toNativeSeparators(AppPaths::dataRoot() + "/" + row.subfolder));
        row.button->setText("Open Folder...");
        break;
    case FileLocation::RomFolder:
        row.path->setText("Next to each game");
        row.button->setText("Open Folder...");
        break;
    case FileLocation::Custom:
        row.path->setText(QDir::toNativeSeparators(row.custom));
        row.button->setText("Browse...");
        break;
    }
    row.button->setEnabled(static_cast<FileLocation>(row.location->currentData().toInt()) != FileLocation::RomFolder);
    if (m_biosStatus)
        refreshBiosStatus();
}

void FilesPage::buttonClicked(Row& row)
{
    if (static_cast<FileLocation>(row.location->currentData().toInt()) == FileLocation::Custom) {
        const QString folder = QFileDialog::getExistingDirectory(this, "Select a Folder", row.custom);
        if (!folder.isEmpty()) {
            row.custom = folder;
            refreshRow(row);
        }
        return;
    }
    const QString folder = AppPaths::dataRoot() + "/" + row.subfolder;
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}
