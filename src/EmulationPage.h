#pragma once

#include <QWidget>

#include "AppSettings.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

// Settings > Emulation: speed control and rewind.
class EmulationPage : public QWidget {
    Q_OBJECT
public:
    explicit EmulationPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    QComboBox* m_method;
    QDoubleSpinBox* m_frameRate;
    QSpinBox* m_maxSkip;
    QSpinBox* m_fastForwardSkip;
    QSpinBox* m_fixedSkip;
    QSpinBox* m_rewindMb;
    QSpinBox* m_rewindInterval;
};
