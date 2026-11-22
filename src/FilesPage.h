#pragma once

#include <QWidget>
#include <array>

#include "AppSettings.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;

// Settings > Files: where saves, states, screenshots and BIOS files live, each one a choice of the data folder next
// to the program, the folder of the game, or a folder of the user's.
class FilesPage : public QWidget {
    Q_OBJECT
public:
    explicit FilesPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    struct Row {
        Row() = default;
        Row(QString label_, QString subfolder_, bool allowRomFolder_)
            : label(std::move(label_)), subfolder(std::move(subfolder_)), allowRomFolder(allowRomFolder_) {}
        QString label;
        QString subfolder; // under the data folder
        bool allowRomFolder = false;
        QComboBox* location = nullptr;
        QLineEdit* path = nullptr;
        QPushButton* button = nullptr;
        QString custom;
    };

    void addLibraryFolder();
    void findBios();
    QString biosFolder() const;
    void refreshBiosStatus();
    void refreshRow(Row& row);
    void buttonClicked(Row& row);

    std::array<Row, 5> m_rows;
    QListWidget* m_libraryFolders = nullptr;
    QLabel* m_biosStatus = nullptr;
};
