#pragma once

#include <QWidget>
#include <utility>
#include <vector>

#include "AppSettings.h"
#include "SystemInfo.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

// Settings > Emulation: which core plays each system, speed control and rewind.
class EmulationPage : public QWidget {
    Q_OBJECT
public:
    explicit EmulationPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    std::vector<std::pair<System, QComboBox*>> m_cores; // one per system
    QComboBox* m_method;
    QDoubleSpinBox* m_frameRate;
    QSpinBox* m_maxSkip;
    QSpinBox* m_fastForwardSkip;
    QSpinBox* m_fixedSkip;
    QSpinBox* m_rewindMb;
    QSpinBox* m_rewindInterval;
};
