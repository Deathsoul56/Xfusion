#pragma once

#include <QTimer>
#include <QWidget>
#include <optional>
#include <vector>

#include "InputConfig.h"
#include "InputManager.h"

class ControllerDiagram;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

// The Controllers settings page: pick a platform group, then click a cell and press a key or a
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
    void rebuildTable();
    void refreshCell(int row, int slot);
    void refreshRow(int row);
    void beginCapture(int row, int slot);
    void endCapture();
    void finishCapture(const InputBinding& binding);
    void pollController();
    void autoAssign();
    void clearRow(int row);
    void clearCell(int row, int slot);
    void showStatus(const QString& text);
    void highlightCurrentRow();
    void syncPadTypeCombo();

    InputManager& m_input;
    InputConfig m_config;
    QComboBox* m_platform;
    QWidget* m_padRow;
    QComboBox* m_padType;
    QLabel* m_status;
    ControllerDiagram* m_diagram;
    QTableWidget* m_table;
    QTimer m_pollTimer;
    QTimer m_timeout;
    std::optional<Capture> m_capture;
    std::vector<PadButton> m_rowButtons;
};
