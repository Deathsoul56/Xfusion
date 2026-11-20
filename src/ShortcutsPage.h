#pragma once

#include <QTimer>
#include <QWidget>
#include <optional>

#include "AppSettings.h"
#include "InputManager.h"

class QTableWidget;

// Settings > Shortcuts: every frontend action with up to four bindings, keys with their modifiers or gamepad
// inputs. A binding used twice is highlighted rather than refused. Edits go to a private copy until OK.
class ShortcutsPage : public QWidget {
    Q_OBJECT
public:
    ShortcutsPage(InputManager& input, const AppSettings& settings, QWidget* parent = nullptr);

    void applyTo(AppSettings& settings) const { settings.shortcuts = m_map; }
    void restoreDefaults();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct Capture {
        int row;
        int slot;
    };

    void refreshCell(int row, int slot);
    void refreshAll();
    void beginCapture(int row, int slot);
    void endCapture();
    void finishCapture(const InputBinding& binding);
    void pollJoystick();

    InputManager& m_input;
    ShortcutMap m_map;
    QTableWidget* m_table;
    QTimer m_pollTimer;
    std::optional<Capture> m_capture;
};
