#pragma once

#include <QDialog>
#include <QMap>
#include <QVector>

#include "SlangPreset.h"

class QDoubleSpinBox;
class QSlider;

// The parameters of the shaders of a preset (the `#pragma parameter` lines), each with a slider and a box, changing the
// picture as they move.
class ShaderParametersDialog : public QDialog {
    Q_OBJECT
public:
    ShaderParametersDialog(const QString& title, const QVector<SlangParameter>& parameters, const QMap<QString, double>& values, QWidget* parent = nullptr);

    QMap<QString, double> values() const;

signals:
    void changed(const QMap<QString, double>& values);

private:
    void setValue(int index, double value, bool fromSlider);

    struct Row {
        SlangParameter parameter;
        QSlider* slider;
        QDoubleSpinBox* box;
    };
    QVector<Row> m_rows;
    bool m_updating = false;
};
