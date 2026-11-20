#pragma once

#include <QTimer>
#include <QWidget>
#include <functional>
#include <optional>
#include <vector>

#include "InputConfig.h"
#include "InputManager.h"

class ControllerDiagram;
class QComboBox;
class QMenu;
class QToolButton;
class QLabel;
class QPushButton;
class QTableWidget;

// The Controllers settings page: pick a platform group and a player, then click a cell and press a key or a
// controller input to bind it. Edits go to a private copy until the dialog is accepted.
class ControllersPage : public QWidget {
    Q_OBJECT
public:
    ControllersPage(InputManager& input, const InputConfig& config, InputGroup initialGroup, QWidget* parent = nullptr);

    const InputConfig& config() const { return m_config; }
    void restoreDefaults();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct Capture {
        PadButton button;
        int slot;
    };

    InputGroup currentGroup() const;
    int currentPlayer() const;
    void rebuildTable();
    void refreshCell(int row, int slot);
    void refreshRow(int row);
    void beginCapture(int row, int slot);
    void endCapture();
    void finishCapture(const InputBinding& binding);
    void pollController();
    void clearRow(int row);
    void clearCell(int row, int slot);
    void showStatus(const QString& text);
    void updateLiveButtons();
    void syncPadTypeCombo();
    void refreshDevices(bool force);
    void rebuildAutoAssignMenu();
    void autoAssign(int slot, const QString& deviceName);

    InputManager& m_input;
    InputConfig m_config;
    QComboBox* m_platform;
    QComboBox* m_ports;
    QComboBox* m_player;
    QToolButton* m_autoAssignButton;
    QMenu* m_autoAssignMenu;
    QLabel* m_raw;
    QComboBox* m_device;
    QWidget* m_padRow;
    QComboBox* m_padType;
    QLabel* m_status;
    ControllerDiagram* m_diagram;
    QTableWidget* m_table;
    QTimer m_pollTimer;
    QTimer m_timeout;
    QTimer m_liveTimer;
    std::optional<Capture> m_capture;
    std::vector<PadButton> m_rowButtons;
    QStringList m_knownDevices;
    int m_liveTicks = 0;
};
