#pragma once

#include <QDialog>
#include <QMap>

class CoreInstaller;
class QLabel;
class QListWidget;
class QPushButton;

// Options > Emulation > Manage Cores...: the versions of each core that are installed, which one is active, and the
// buttons to look for a newer build on the libretro buildbot, install it, go back to another version or delete one
// (ADR-0027). The active version is the one a game uses the next time it opens.
class CoresDialog : public QDialog {
    Q_OBJECT
public:
    explicit CoresDialog(QWidget* parent = nullptr);

private:
    struct Panel {
        QString coreId;
        QLabel* status = nullptr;
        QListWidget* versions = nullptr;
        QPushButton* use = nullptr;
        QPushButton* remove = nullptr;
        QPushButton* check = nullptr;
        QPushButton* install = nullptr;
    };

    void refresh();
    void setBusy(bool busy);
    void useSelected(const Panel& panel);
    void removeSelected(const Panel& panel);
    void say(const QString& text);

    CoreInstaller* m_installer;
    QList<Panel> m_panels;
    QLabel* m_message;
    QMap<QString, QString> m_serverDates; // from the last check, by core id
};
