#include "GeneralPage.h"

#include "FormAlign.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Theme.h"

GeneralPage::GeneralPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_fullscreenOnOpen(new QCheckBox("Switch to fullscreen after opening a game", this))
    , m_pauseUnfocused(new QCheckBox("Pause emulation when window is not focused", this))
    , m_backgroundGamepad(new QCheckBox("Allow gamepad input while window is not focused", this))
    , m_confirmSaveLoad(new QCheckBox("Ask for confirmation when saving/loading over an existing game position", this))
    , m_saveStateOnClose(new QCheckBox("Save State On Game Close (automatically loaded next time)", this))
    , m_theme(new QComboBox(this))
    , m_showFps(new QCheckBox("Show frame rate", this))
    , m_showIndicators(new QCheckBox("Show fast-forward, rewind and pause icons", this))
    , m_showPressed(new QCheckBox("Show pressed buttons", this))
    , m_showTime(new QCheckBox("Show the time", this))
    , m_showRomInfo(new QCheckBox("Show game info when a game loads", this))
    , m_sramInterval(new QSpinBox(this))
{
    m_pauseUnfocused->setToolTip("Also pauses while the window is minimized, whatever this says.");
    m_backgroundGamepad->setToolTip("The keyboard is still ignored while another window has the focus.");
    m_saveStateOnClose->setToolTip("Written when you use File > Close Game, to <game>.resume.state; loaded the next time that game opens.");
    m_theme->addItems(Theme::names());
    m_sramInterval->setRange(0, 1000);
    m_sramInterval->setSuffix(" seconds");
    m_sramInterval->setToolTip("0 saves the battery RAM only when the game is closed or another one is opened.");

    auto* general = new QGroupBox("General", this);
    auto* generalLayout = new QVBoxLayout(general);
    for (QCheckBox* box : {m_fullscreenOnOpen, m_pauseUnfocused, m_backgroundGamepad, m_confirmSaveLoad, m_saveStateOnClose})
        generalLayout->addWidget(box);

    auto* appearance = new QGroupBox("Appearance", this);
    auto* appearanceLayout = new QFormLayout(appearance);
    appearanceLayout->addRow("Theme:", m_theme);

    auto* overlays = new QGroupBox("Overlays", this);
    auto* overlaysLayout = new QVBoxLayout(overlays);
    for (QCheckBox* box : {m_showFps, m_showIndicators, m_showPressed, m_showTime, m_showRomInfo})
        overlaysLayout->addWidget(box);

    auto* sram = new QGroupBox("SRAM", this);
    auto* sramLayout = new QFormLayout(sram);
    sramLayout->addRow("Save SRAM to disk every:", m_sramInterval);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(general);
    layout->addWidget(appearance);
    layout->addWidget(overlays);
    layout->addWidget(sram);
    layout->addStretch(1);
    alignFormLabels(this);

    setValues(settings);
}

void GeneralPage::setValues(const AppSettings& s)
{
    m_fullscreenOnOpen->setChecked(s.fullscreenOnOpen);
    m_pauseUnfocused->setChecked(s.pauseWhenUnfocused);
    m_backgroundGamepad->setChecked(s.backgroundGamepad);
    m_confirmSaveLoad->setChecked(s.confirmSaveLoad);
    m_saveStateOnClose->setChecked(s.saveStateOnClose);
    m_theme->setCurrentIndex(std::min(s.theme, m_theme->count() - 1));
    m_showFps->setChecked(s.showFps);
    m_showIndicators->setChecked(s.showIndicators);
    m_showPressed->setChecked(s.showPressedKeys);
    m_showTime->setChecked(s.showTime);
    m_showRomInfo->setChecked(s.showRomInfo);
    m_sramInterval->setValue(s.sramInterval);
}

void GeneralPage::applyTo(AppSettings& s) const
{
    s.fullscreenOnOpen = m_fullscreenOnOpen->isChecked();
    s.pauseWhenUnfocused = m_pauseUnfocused->isChecked();
    s.backgroundGamepad = m_backgroundGamepad->isChecked();
    s.confirmSaveLoad = m_confirmSaveLoad->isChecked();
    s.saveStateOnClose = m_saveStateOnClose->isChecked();
    s.theme = m_theme->currentIndex();
    s.showFps = m_showFps->isChecked();
    s.showIndicators = m_showIndicators->isChecked();
    s.showPressedKeys = m_showPressed->isChecked();
    s.showTime = m_showTime->isChecked();
    s.showRomInfo = m_showRomInfo->isChecked();
    s.sramInterval = m_sramInterval->value();
}
