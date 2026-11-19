#pragma once

#include <QDialog>

#include "InputConfig.h"
#include "InputManager.h"

class ControllersPage;

// Settings window: a page list on the left and the selected page on the right. Only the Controllers
// page exists so far; the others (Display, Sound, ...) arrive with their phases.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(InputManager& input, const InputConfig& config, InputGroup initialGroup, QWidget* parent = nullptr);

    InputConfig inputConfig() const;

private:
    ControllersPage* m_controllers;
};
