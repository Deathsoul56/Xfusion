#include "DisplayPage.h"

#include "FormAlign.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "VideoFilters.h"

DisplayPage::DisplayPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_maintainAspect(new QCheckBox("Maintain aspect ratio", this))
    , m_integer(new QCheckBox("Use integer scaling", this))
    , m_stretch(new QCheckBox("Stretch image", this))
    , m_smooth(new QCheckBox("Bilinear filter", this))
    , m_aspect(new QComboBox(this))
    , m_filter(new QComboBox(this))
    , m_messages(new QComboBox(this))
    , m_osdSize(new QSpinBox(this))
    , m_corner(new QComboBox(this))
{
    m_maintainAspect->setToolTip("Keep the screen at the requested proportions for width and height.");
    m_integer->setToolTip("When scaling up, only use integer multiples of the original height.");
    m_stretch->setToolTip("Stretch the emulated image to fill the display area. Off draws it at its own size, centred.");
    m_smooth->setToolTip("Smooth screen output. Off keeps hard pixel edges.");
    m_aspect->addItem("Core default", static_cast<int>(AspectMode::Core));
    m_aspect->addItem("4:3 - Classic display aspect", static_cast<int>(AspectMode::FourThree));
    m_aspect->addItem("Square pixels - the frame's own proportions", static_cast<int>(AspectMode::Square));
    for (const VideoFilterInfo& info : allVideoFilters())
        m_filter->addItem(info.label, static_cast<int>(info.id));
    m_filter->setToolTip("CPU filters run on every frame: the larger ones (xBRZ 5x and 6x, HQ4x) need a fast processor. "
                         "NTSC filters emulate an analogue signal and were made for the SNES, so on SEGA they are an approximation.");
    m_messages->addItem("Onscreen - over the picture", static_cast<int>(MessageMode::Onscreen));
    m_messages->addItem("Don't display messages", static_cast<int>(MessageMode::None));
    m_osdSize->setRange(8, 256);
    m_osdSize->setSuffix(" pt");
    m_corner->addItem("Bottom left", static_cast<int>(OsdCorner::BottomLeft));
    m_corner->addItem("Bottom right", static_cast<int>(OsdCorner::BottomRight));
    m_corner->addItem("Top left", static_cast<int>(OsdCorner::TopLeft));
    m_corner->addItem("Top right", static_cast<int>(OsdCorner::TopRight));

    auto* output = new QGroupBox("Display Output", this);
    auto* outputLayout = new QFormLayout(output);
    outputLayout->addRow("Driver:", new QLabel("Qt Software", output));
    outputLayout->addRow(m_smooth);

    auto* scaling = new QGroupBox("Scaling and Aspect Ratio", this);
    auto* scalingLayout = new QFormLayout(scaling);
    scalingLayout->addRow(m_maintainAspect);
    scalingLayout->addRow(m_integer);
    scalingLayout->addRow(m_stretch);
    scalingLayout->addRow("Aspect ratio:", m_aspect);

    auto* filters = new QGroupBox("Software Filters", this);
    auto* filtersLayout = new QFormLayout(filters);
    filtersLayout->addRow("Filter:", m_filter);

    auto* messages = new QGroupBox("Messages", this);
    auto* messagesLayout = new QFormLayout(messages);
    messagesLayout->addRow("Display messages:", m_messages);
    messagesLayout->addRow("Onscreen display size:", m_osdSize);
    messagesLayout->addRow("Message corner:", m_corner);

    auto* hint = new QLabel("Fullscreen: see Options > Shortcuts (Esc always leaves it). These settings apply to every system.", this);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(output);
    layout->addWidget(scaling);
    layout->addWidget(filters);
    layout->addWidget(messages);
    layout->addWidget(hint);
    layout->addStretch(1);
    alignFormLabels(this);

    setValues(settings);
}

void DisplayPage::setValues(const AppSettings& s)
{
    m_maintainAspect->setChecked(s.maintainAspect);
    m_integer->setChecked(s.integerScale);
    m_stretch->setChecked(s.stretch);
    m_smooth->setChecked(s.smooth);
    m_aspect->setCurrentIndex(m_aspect->findData(static_cast<int>(s.aspect)));
    m_filter->setCurrentIndex(m_filter->findData(static_cast<int>(s.filter)));
    m_messages->setCurrentIndex(m_messages->findData(static_cast<int>(s.messages)));
    m_osdSize->setValue(s.osdSize);
    m_corner->setCurrentIndex(m_corner->findData(static_cast<int>(s.osdCorner)));
}

void DisplayPage::applyTo(AppSettings& s) const
{
    s.maintainAspect = m_maintainAspect->isChecked();
    s.integerScale = m_integer->isChecked();
    s.stretch = m_stretch->isChecked();
    s.smooth = m_smooth->isChecked();
    s.aspect = static_cast<AspectMode>(m_aspect->currentData().toInt());
    s.filter = static_cast<VideoFilter>(m_filter->currentData().toInt());
    s.messages = static_cast<MessageMode>(m_messages->currentData().toInt());
    s.osdSize = m_osdSize->value();
    s.osdCorner = static_cast<OsdCorner>(m_corner->currentData().toInt());
}
