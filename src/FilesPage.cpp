#include "FilesPage.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "AppPaths.h"

FilesPage::FilesPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
{
    m_rows = {{{"Save RAM (SRAM)", "saves", true}, {"Save states", "states", true}, {"Screenshots", "screenshots", true}, {"BIOS", "system", false}}};

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

    auto* hint = new QLabel("Saves, states and screenshots keep one folder per system, so games with the same name on different systems never "
                            "overwrite each other. Data Folder is next to the program (or XFUSION_DATA_DIR). Game Folder puts the files "
                            "beside each game instead. Changes apply the next time a game opens.",
                            this);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->addWidget(hint);
    layout->addStretch(1);

    setValues(settings);
}

void FilesPage::setValues(const AppSettings& s)
{
    const FolderSetting* folders[] = {&s.sramFolder, &s.stateFolder, &s.screenshotFolder, &s.biosFolder};
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        Row& row = m_rows[i];
        row.custom = folders[i]->custom;
        int index = row.location->findData(static_cast<int>(folders[i]->location));
        row.location->setCurrentIndex(std::max(0, index));
        refreshRow(row);
    }
}

void FilesPage::applyTo(AppSettings& s) const
{
    FolderSetting* folders[] = {&s.sramFolder, &s.stateFolder, &s.screenshotFolder, &s.biosFolder};
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        const Row& row = m_rows[i];
        folders[i]->location = static_cast<FileLocation>(row.location->currentData().toInt());
        folders[i]->custom = row.custom;
    }
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
