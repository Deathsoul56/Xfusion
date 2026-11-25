#pragma once

#include "ScopedPage.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

// Settings > Sound: volume, output device, buffer and what to mute.
class SoundPage : public ScopedPage {
    Q_OBJECT
public:
    explicit SoundPage(const AppSettings& settings, QWidget* parent = nullptr);

protected:
    void loadControls(const AppSettings& settings) override;
    void readControls(AppSettings& settings) const override;

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
