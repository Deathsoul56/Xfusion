#pragma once

#include "ScopedPage.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

// Settings > Display: scaling and aspect ratio, filters and on-screen messages.
class DisplayPage : public ScopedPage {
    Q_OBJECT
public:
    explicit DisplayPage(const AppSettings& settings, QWidget* parent = nullptr);

protected:
    void loadControls(const AppSettings& settings) override;
    void readControls(AppSettings& settings) const override;

private:
    QCheckBox* m_maintainAspect;
    QCheckBox* m_integer;
    QCheckBox* m_stretch;
    QCheckBox* m_smooth;
    QComboBox* m_aspect;
    QComboBox* m_filter;
    QCheckBox* m_gpuFilters;
    QComboBox* m_shader;
    QLineEdit* m_preset;
    QComboBox* m_driver;
    QComboBox* m_messages;
    QSpinBox* m_osdSize;
    QComboBox* m_corner;
};
