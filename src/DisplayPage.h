#pragma once

#include <QWidget>

#include "AppSettings.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

// Settings > Display: scaling and aspect ratio, filters and on-screen messages.
class DisplayPage : public QWidget {
    Q_OBJECT
public:
    explicit DisplayPage(const AppSettings& settings, QWidget* parent = nullptr);

    void setValues(const AppSettings& settings);
    void applyTo(AppSettings& settings) const;
    void restoreDefaults() { setValues(AppSettings()); }

private:
    QCheckBox* m_maintainAspect;
    QCheckBox* m_integer;
    QCheckBox* m_stretch;
    QCheckBox* m_smooth;
    QComboBox* m_aspect;
    QComboBox* m_filter;
    QComboBox* m_messages;
    QSpinBox* m_osdSize;
    QComboBox* m_corner;
};
