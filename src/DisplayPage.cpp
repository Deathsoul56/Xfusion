#include "DisplayPage.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <algorithm>

#include "AppPaths.h"

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
    : ScopedPage("display", settings, parent)
    , m_maintainAspect(new QCheckBox("Maintain aspect ratio", this))
    , m_integer(new QCheckBox("Use integer scaling", this))
    , m_stretch(new QCheckBox("Stretch image", this))
    , m_smooth(new QCheckBox("Bilinear filter", this))
    , m_aspect(new QComboBox(this))
    , m_filter(new QComboBox(this))
    , m_gpuFilters(new QCheckBox("Run the filters on the GPU when they can", this))
    , m_shader(new QComboBox(this))
    , m_preset(new QLineEdit(this))
    , m_driver(new QComboBox(this))
    , m_messages(new QComboBox(this))
    , m_osdSize(new QSpinBox(this))
    , m_corner(new QComboBox(this))
{
    m_maintainAspect->setToolTip("Keep the picture at the proportions chosen below, fitted inside the window. Off fills the whole area, as Stretch image does.");
    m_integer->setToolTip("When scaling up, only use integer multiples of the original height.");
    m_stretch->setToolTip("Stretch the picture to fill the whole display area, whatever its proportions. Off fits it inside the window as large as it goes.");
    m_smooth->setToolTip("Smooth screen output. Off keeps hard pixel edges.");
    m_aspect->addItem("Core default", static_cast<int>(AspectMode::Core));
    m_aspect->addItem("4:3 - Classic display aspect", static_cast<int>(AspectMode::FourThree));
    m_aspect->addItem("Square pixels - the frame's own proportions", static_cast<int>(AspectMode::Square));
    for (const VideoFilterInfo& info : allVideoFilters())
        m_filter->addItem(info.label, static_cast<int>(info.id));
    m_filter->setToolTip("CPU filters run on every frame: the larger ones (xBRZ 5x and 6x, HQ4x) need a fast processor. "
                         "NTSC filters emulate an analogue signal and were made for the SNES, so on SEGA they are an approximation.");
    m_driver->addItem("Automatic - the GPU, or software if it cannot", static_cast<int>(VideoDriver::Auto));
    m_driver->addItem("Qt Software", static_cast<int>(VideoDriver::Software));
    m_driver->addItem("Direct3D 11", static_cast<int>(VideoDriver::Direct3D11));
    m_driver->addItem("Direct3D 12", static_cast<int>(VideoDriver::Direct3D12));
    m_driver->addItem("Vulkan", static_cast<int>(VideoDriver::Vulkan));
    m_driver->addItem("OpenGL", static_cast<int>(VideoDriver::OpenGL));
    m_driver->setToolTip("How the picture is drawn. Takes effect the next time Xfusion starts. Automatic uses the GPU (Direct3D 11 on Windows) and falls back to software, saying so, if the GPU cannot draw it.");
    m_gpuFilters->setToolTip("With the GPU driver, xBRZ, HQx, EPX, Scanlines and the plain scalers are done by the graphics card and not by the processor. NTSC, 2xSaI, Super 2xSaI and Super Eagle only have a version for the processor. "
                             "It is not used together with a GPU shader below, which needs the filter done first.");
    m_shader->addItem("None", static_cast<int>(VideoShader::None));
    m_shader->addItem("Scanlines", static_cast<int>(VideoShader::Scanlines));
    m_shader->addItem("CRT - scanlines, grille and vignette", static_cast<int>(VideoShader::Crt));
    m_shader->addItem("Sharp bilinear", static_cast<int>(VideoShader::SharpBilinear));
    m_shader->addItem("LCD grid", static_cast<int>(VideoShader::Lcd));
    m_shader->addItem("Preset file (.slangp)", static_cast<int>(VideoShader::Preset));
    m_preset->setReadOnly(true);
    m_preset->setPlaceholderText("RetroArch slang preset, for example crt-easymode.slangp");
    m_preset->setToolTip("A shader preset in the format of RetroArch (.slangp). Put the shaders in the shaders folder next to the program, or anywhere else. Xfusion does not ship any.");
    auto* presetRow = new QWidget(this);
    auto* presetLayout = new QHBoxLayout(presetRow);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    auto* browse = new QPushButton("Browse...", presetRow);
    auto* clearPreset = new QPushButton("Clear", presetRow);
    presetLayout->addWidget(m_preset, 1);
    presetLayout->addWidget(browse);
    presetLayout->addWidget(clearPreset);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString start = m_preset->text().isEmpty() ? AppPaths::shadersDir() : QFileInfo(m_preset->text()).absolutePath();
        const QString file = QFileDialog::getOpenFileName(this, "Shader preset", start, "Shader presets (*.slangp);;All files (*)");
        if (file.isEmpty())
            return;
        m_preset->setText(file);
        m_shader->setCurrentIndex(m_shader->findData(static_cast<int>(VideoShader::Preset)));
    });
    connect(clearPreset, &QPushButton::clicked, this, [this] { m_preset->clear(); });
    m_shader->setToolTip("Drawn by the GPU at the resolution of the screen, after the software filter. It has no effect with the Qt Software driver.");
    m_messages->addItem("Onscreen - over the picture", static_cast<int>(MessageMode::Onscreen));
    m_messages->addItem("Don't display messages", static_cast<int>(MessageMode::None));
    m_osdSize->setRange(8, 256);
    m_osdSize->setSuffix(" px");
    m_corner->addItem("Bottom left", static_cast<int>(OsdCorner::BottomLeft));
    m_corner->addItem("Bottom right", static_cast<int>(OsdCorner::BottomRight));
    m_corner->addItem("Top left", static_cast<int>(OsdCorner::TopLeft));
    m_corner->addItem("Top right", static_cast<int>(OsdCorner::TopRight));

    auto* output = new QGroupBox("Display Output", this);
    auto* outputLayout = new QFormLayout(output);
    outputLayout->addRow("Driver:", globalOnly(m_driver));
    outputLayout->addRow(row(m_smooth, "smooth"));

    auto* scaling = new QGroupBox("Scaling and Aspect Ratio", this);
    auto* scalingLayout = new QFormLayout(scaling);
    scalingLayout->addRow(row(m_maintainAspect, "maintain_aspect"));
    scalingLayout->addRow(row(m_integer, "integer_scale"));
    scalingLayout->addRow(row(m_stretch, "stretch"));
    scalingLayout->addRow("Aspect ratio:", row(m_aspect, "aspect"));

    auto* filters = new QGroupBox("Software Filters", this);
    auto* filtersLayout = new QFormLayout(filters);
    filtersLayout->addRow("Filter:", row(m_filter, "filter"));
    filtersLayout->addRow(row(m_gpuFilters, "gpu_filters"));

    auto* shaders = new QGroupBox("GPU Shaders", this);
    auto* shadersLayout = new QFormLayout(shaders);
    shadersLayout->addRow("Shader:", row(m_shader, "shader"));
    shadersLayout->addRow("Preset:", row(presetRow, "shader_preset"));

    auto* messages = new QGroupBox("Messages", this);
    auto* messagesLayout = new QFormLayout(messages);
    messagesLayout->addRow("Display messages:", row(m_messages, "messages"));
    messagesLayout->addRow("Onscreen display size:", row(m_osdSize, "osd_size"));
    messagesLayout->addRow("Message corner:", row(m_corner, "osd_corner"));

    auto* hint = new QLabel("Fullscreen: see Options > Shortcuts (Esc always leaves it). Choose a system above to give it settings of its own.", this);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scopeSelector());
    layout->addWidget(output);
    layout->addWidget(scaling);
    layout->addWidget(filters);
    layout->addWidget(shaders);
    layout->addWidget(messages);
    layout->addWidget(hint);
    layout->addStretch(1);
    alignFormLabels(this);

    start();
}

void DisplayPage::loadControls(const AppSettings& s)
{
    m_maintainAspect->setChecked(s.maintainAspect);
    m_integer->setChecked(s.integerScale);
    m_stretch->setChecked(s.stretch);
    m_smooth->setChecked(s.smooth);
    m_aspect->setCurrentIndex(m_aspect->findData(static_cast<int>(s.aspect)));
    m_filter->setCurrentIndex(m_filter->findData(static_cast<int>(s.filter)));
    m_gpuFilters->setChecked(s.gpuFilters);
    m_shader->setCurrentIndex(m_shader->findData(static_cast<int>(s.shader)));
    m_preset->setText(s.shaderPreset);
    m_driver->setCurrentIndex(std::max(0, m_driver->findData(static_cast<int>(s.videoDriver))));
    m_messages->setCurrentIndex(m_messages->findData(static_cast<int>(s.messages)));
    m_osdSize->setValue(s.osdSize);
    m_corner->setCurrentIndex(m_corner->findData(static_cast<int>(s.osdCorner)));
}

void DisplayPage::readControls(AppSettings& s) const
{
    s.maintainAspect = m_maintainAspect->isChecked();
    s.integerScale = m_integer->isChecked();
    s.stretch = m_stretch->isChecked();
    s.smooth = m_smooth->isChecked();
    s.aspect = static_cast<AspectMode>(m_aspect->currentData().toInt());
    s.filter = static_cast<VideoFilter>(m_filter->currentData().toInt());
    s.gpuFilters = m_gpuFilters->isChecked();
    s.shader = static_cast<VideoShader>(m_shader->currentData().toInt());
    s.shaderPreset = m_preset->text();
    s.videoDriver = static_cast<VideoDriver>(m_driver->currentData().toInt());
    s.messages = static_cast<MessageMode>(m_messages->currentData().toInt());
    s.osdSize = m_osdSize->value();
    s.osdCorner = static_cast<OsdCorner>(m_corner->currentData().toInt());
}
