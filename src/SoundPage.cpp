#include "SoundPage.h"

#include "FormAlign.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "AudioOutput.h"

namespace {

QWidget* sliderRow(QSlider* slider, QLabel* text, QWidget* parent)
{
    slider->setRange(0, AppSettings::kMaxVolume);
    slider->setPageStep(10);
    text->setMinimumWidth(text->fontMetrics().horizontalAdvance("100%") + 8);
    text->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QObject::connect(slider, &QSlider::valueChanged, text, [text](int value) { text->setText(QString("%1%").arg(value)); });
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(slider, 1);
    layout->addWidget(text);
    return row;
}

}

SoundPage::SoundPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_volume(new QSlider(Qt::Horizontal, this))
    , m_volumeText(new QLabel(this))
    , m_volumeFast(new QSlider(Qt::Horizontal, this))
    , m_volumeFastText(new QLabel(this))
    , m_device(new QComboBox(this))
    , m_rate(new QComboBox(this))
    , m_buffer(new QSpinBox(this))
    , m_muted(new QCheckBox("Mute all sound", this))
    , m_muteFast(new QCheckBox("Mute sound during fast-forward or rewind", this))
    , m_muteStep(new QCheckBox("Mute sound during Frame Advance", this))
{
    m_device->addItem("Default", QString());
    for (const QString& name : AudioOutput::playbackDevices())
        m_device->addItem(name, name);
    m_device->setToolTip("The playback device. Takes effect the next time a game opens.");
    m_rate->addItem("Same as the core", 0);
    for (int rate : {22050, 32000, 44100, 48000, 96000})
        m_rate->addItem(QString("%1 Hz").arg(rate), rate);
    m_rate->setToolTip("The rate the device runs at; the core's sound is converted to it. Takes effect the next time a game opens.");
    m_buffer->setRange(16, 256);
    m_buffer->setSingleStep(16);
    m_buffer->setSuffix(" ms");
    m_buffer->setToolTip("Ideally about two frames, around 32 ms. The audio kept queued, and so the latency, is half of this.");

    auto* volume = new QGroupBox("Volume", this);
    auto* volumeLayout = new QFormLayout(volume);
    volumeLayout->addRow("Regular:", sliderRow(m_volume, m_volumeText, volume));
    volumeLayout->addRow("Fast-Forward:", sliderRow(m_volumeFast, m_volumeFastText, volume));

    auto* output = new QGroupBox("Sound Output", this);
    auto* outputLayout = new QFormLayout(output);
    outputLayout->addRow("Driver:", new QLabel("SDL", output));
    outputLayout->addRow("Device:", m_device);
    outputLayout->addRow("Playback rate:", m_rate);
    outputLayout->addRow("Buffer size:", m_buffer);

    auto* mute = new QGroupBox("Mute", this);
    auto* muteLayout = new QVBoxLayout(mute);
    for (QCheckBox* box : {m_muted, m_muteFast, m_muteStep})
        muteLayout->addWidget(box);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(volume);
    layout->addWidget(output);
    layout->addWidget(mute);
    layout->addStretch(1);
    alignFormLabels(this);

    setValues(settings);
}

void SoundPage::setValues(const AppSettings& s)
{
    m_volume->setValue(s.volume);
    m_volumeText->setText(QString("%1%").arg(s.volume));
    m_volumeFast->setValue(s.volumeFastForward);
    m_volumeFastText->setText(QString("%1%").arg(s.volumeFastForward));
    int device = m_device->findData(s.audioDevice);
    if (device < 0 && !s.audioDevice.isEmpty()) {
        m_device->addItem(QString("%1 (not connected)").arg(s.audioDevice), s.audioDevice);
        device = m_device->count() - 1;
    }
    m_device->setCurrentIndex(std::max(0, device));
    int rate = m_rate->findData(s.playbackRate);
    if (rate < 0) {
        m_rate->addItem(QString("%1 Hz").arg(s.playbackRate), s.playbackRate);
        rate = m_rate->count() - 1;
    }
    m_rate->setCurrentIndex(rate);
    m_buffer->setValue(s.bufferMs);
    m_muted->setChecked(s.muted);
    m_muteFast->setChecked(s.muteDuringFastForward);
    m_muteStep->setChecked(s.muteDuringFrameAdvance);
}

void SoundPage::applyTo(AppSettings& s) const
{
    s.volume = m_volume->value();
    s.volumeFastForward = m_volumeFast->value();
    s.audioDevice = m_device->currentData().toString();
    s.playbackRate = m_rate->currentData().toInt();
    s.bufferMs = m_buffer->value();
    s.muted = m_muted->isChecked();
    s.muteDuringFastForward = m_muteFast->isChecked();
    s.muteDuringFrameAdvance = m_muteStep->isChecked();
}
