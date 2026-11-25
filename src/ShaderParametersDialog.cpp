#include "ShaderParametersDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {

int decimalsFor(double step)
{
    int decimals = 0;
    while (decimals < 6 && std::fabs(step * std::pow(10.0, decimals) - std::round(step * std::pow(10.0, decimals))) > 1e-9)
        ++decimals;
    return std::max(decimals, 2);
}

}

ShaderParametersDialog::ShaderParametersDialog(const QString& title, const QVector<SlangParameter>& parameters, const QMap<QString, double>& values, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    resize(560, 520);
    auto* body = new QWidget(this);
    auto* grid = new QGridLayout(body);
    int row = 0;
    for (const SlangParameter& parameter : parameters) {
        const double step = parameter.step > 0 ? parameter.step : (parameter.maximum - parameter.minimum) / 100.0;
        auto* label = new QLabel(parameter.description.isEmpty() ? parameter.id : parameter.description, body);
        label->setToolTip(parameter.id);
        auto* slider = new QSlider(Qt::Horizontal, body);
        const int steps = std::max(1, int(std::lround((parameter.maximum - parameter.minimum) / step)));
        slider->setRange(0, steps);
        auto* box = new QDoubleSpinBox(body);
        box->setDecimals(decimalsFor(step));
        box->setRange(parameter.minimum, parameter.maximum);
        box->setSingleStep(step);
        const int index = int(m_rows.size());
        m_rows.append({parameter, slider, box});
        grid->addWidget(label, row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(box, row, 2);
        connect(slider, &QSlider::valueChanged, this, [this, index, step](int position) { setValue(index, m_rows[index].parameter.minimum + position * step, true); });
        connect(box, &QDoubleSpinBox::valueChanged, this, [this, index](double value) { setValue(index, value, false); });
        setValue(index, values.value(parameter.id, parameter.initial), false);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_rows.size(); ++i)
            setValue(i, m_rows[i].parameter.initial, false);
    });
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);
}

void ShaderParametersDialog::setValue(int index, double value, bool fromSlider)
{
    if (m_updating)
        return;
    m_updating = true;
    Row& row = m_rows[index];
    value = std::clamp(value, row.parameter.minimum, row.parameter.maximum);
    const double step = row.parameter.step > 0 ? row.parameter.step : (row.parameter.maximum - row.parameter.minimum) / 100.0;
    if (!fromSlider)
        row.slider->setValue(int(std::lround((value - row.parameter.minimum) / step)));
    row.box->setValue(value);
    m_updating = false;
    emit changed(values());
}

QMap<QString, double> ShaderParametersDialog::values() const
{
    QMap<QString, double> result;
    for (const Row& row : m_rows)
        result[row.parameter.id] = row.box->value();
    return result;
}
