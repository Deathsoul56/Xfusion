#pragma once

#include <QWidget>

#include "AppSettings.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

// Settings > Sound: volume, output device, buffer and what to mute.
class SoundPage : public QWidget {
    Q_OBJECT
public:
    explicit SoundPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    QSlider* m_volume;
    QLabel* m_volumeText;
    QSlider* m_volumeFast;
    QLabel* m_volumeFastText;
    QComboBox* m_device;
    QComboBox* m_rate;
    QSpinBox* m_buffer;
    QCheckBox* m_muted;
    QCheckBox* m_muteFast;
    QCheckBox* m_muteStep;
};
