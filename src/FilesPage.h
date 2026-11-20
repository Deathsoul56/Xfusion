#pragma once

#include <QWidget>
#include <array>

#include "AppSettings.h"

class QComboBox;
class QLineEdit;
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
        QString label;
        QString subfolder; // under the data folder
        bool allowRomFolder;
        QComboBox* location = nullptr;
        QLineEdit* path = nullptr;
        QPushButton* button = nullptr;
        QString custom;
    };

    void refreshRow(Row& row);
    void buttonClicked(Row& row);

    std::array<Row, 4> m_rows;
};
