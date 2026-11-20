#pragma once

#include <QWidget>

#include "AppSettings.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

// Settings > General: window behaviour, appearance, on-screen overlays and the battery save interval.
class GeneralPage : public QWidget {
    Q_OBJECT
public:
    explicit GeneralPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    QCheckBox* m_fullscreenOnOpen;
    QCheckBox* m_pauseUnfocused;
    QCheckBox* m_backgroundGamepad;
    QCheckBox* m_confirmSaveLoad;
    QCheckBox* m_saveStateOnClose;
    QComboBox* m_theme;
    QCheckBox* m_showFps;
    QCheckBox* m_showIndicators;
    QCheckBox* m_showPressed;
    QCheckBox* m_showTime;
    QCheckBox* m_showRomInfo;
    QSpinBox* m_sramInterval;
};
