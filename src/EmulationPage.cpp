#include "EmulationPage.h"

#include "CoreRules.h"
#include "FormAlign.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <algorithm>
#include <QSpinBox>
#include <QVBoxLayout>

EmulationPage::EmulationPage(const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_method(new QComboBox(this))
    , m_frameRate(new QDoubleSpinBox(this))
    , m_maxSkip(new QSpinBox(this))
    , m_fastForwardSkip(new QSpinBox(this))
    , m_fixedSkip(new QSpinBox(this))
    , m_rewindMb(new QSpinBox(this))
    , m_rewindInterval(new QSpinBox(this))
{
    m_method->addItem("Sound Synchronization - Wait on the sound buffer", static_cast<int>(SpeedMethod::SoundSync));
    m_method->addItem("Timer - Match the frame rate configured below", static_cast<int>(SpeedMethod::Timer));
    m_method->addItem("Timer with Frame Skipping - Skip frames if needed", static_cast<int>(SpeedMethod::TimerFrameskip));
    m_method->addItem("None - Run unthrottled", static_cast<int>(SpeedMethod::Unlimited));
    m_method->setToolTip("Sound Synchronization needs the sound to be on; without it the timer is used.");
    m_frameRate->setRange(0, 1000);
    m_frameRate->setDecimals(4);
    m_frameRate->setSuffix(" fps");
    m_frameRate->setToolTip("0 follows the core's own rate (60 fps NTSC, 50 fps PAL). The Speed Up and Speed Down shortcuts change this.");
    m_maxSkip->setRange(0, 59);
    m_maxSkip->setSuffix(" frames");
    m_maxSkip->setToolTip("Most frames \"Timer with Frame Skipping\" will skip in a row to keep up. 0 = no limit.");
    m_fastForwardSkip->setRange(1, 15);
    m_fastForwardSkip->setSuffix(" frames");
    m_fastForwardSkip->setToolTip("Fast-forward draws one frame out of every N + 1 and runs N + 1 times faster.");
    m_fixedSkip->setRange(0, 59);
    m_fixedSkip->setSuffix(" frames");
    m_fixedSkip->setToolTip("Always skip this many frames per drawn frame, whatever the speed control above says. 0 disables this. "
                            "With Sound Synchronization it only saves drawing work: the speed stays the same.");
    m_rewindMb->setRange(0, 1024);
    m_rewindMb->setSuffix(" MB");
    m_rewindMb->setToolTip("Memory for the rewind history. 0 turns rewind off. How many seconds fit depends on the game.");
    m_rewindInterval->setRange(1, 99);
    m_rewindInterval->setToolTip("Frames between snapshots. Larger values go back further and rewind faster.");

    // Two cores play the systems (ADR-0024). Each system uses its default unless one is chosen here; a core that is
    // not installed is replaced by another that can play the system.
    auto* cores = new QGroupBox("Cores", this);
    auto* coresLayout = new QFormLayout(cores);
    for (const SystemDef& def : allSystems()) {
        auto* combo = new QComboBox(cores);
        combo->addItem(QString("Default (%1)").arg(coreInfo(defaultCoreFor(def.id))->name), QString());
        for (const CoreInfo& core : allCores())
            if (core.supports(def.id))
                combo->addItem(core.name, core.id);
        combo->setToolTip(QString("The core that plays %1 games. Applies the next time a game opens.").arg(def.folder));
        coresLayout->addRow(def.folder + ":", combo);
        m_cores.emplace_back(def.id, combo);
    }
    auto* coresNote = new QLabel("Sega CD discs with .mp3 audio tracks go to PicoDrive when the default is used: Genesis Plus GX cannot play "
                                 "them. Save states are not shared between cores.", cores);
    coresNote->setWordWrap(true);
    coresNote->setEnabled(false);
    coresLayout->addRow(coresNote);

    auto* speed = new QGroupBox("Emulation Speed", this);
    auto* speedLayout = new QFormLayout(speed);
    speedLayout->addRow("Speed control method:", m_method);
    speedLayout->addRow("Frame rate:", m_frameRate);
    auto* note = new QLabel("Set frame rate to 0 to follow the core's own rate (NTSC or PAL).", speed);
    note->setEnabled(false);
    speedLayout->addRow(note);
    speedLayout->addRow("Skip at most:", m_maxSkip);
    speedLayout->addRow("Fast-forward frame skip:", m_fastForwardSkip);
    speedLayout->addRow("Force frame skip:", m_fixedSkip);

    auto* rewind = new QGroupBox("Rewind", this);
    auto* rewindLayout = new QFormLayout(rewind);
    rewindLayout->addRow("Rewind buffer size (0 to disable):", m_rewindMb);
    rewindLayout->addRow("Frames between steps:", m_rewindInterval);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(cores);
    layout->addWidget(speed);
    layout->addWidget(rewind);
    layout->addStretch(1);
    alignFormLabels(this);

    setValues(settings);
}

void EmulationPage::setValues(const AppSettings& s)
{
    m_method->setCurrentIndex(m_method->findData(static_cast<int>(s.speedMethod)));
    m_frameRate->setValue(s.fixedFrameRate);
    m_maxSkip->setValue(s.maxFrameSkip);
    m_fastForwardSkip->setValue(s.fastForwardSkip);
    m_fixedSkip->setValue(s.fixedFrameSkip);
    m_rewindMb->setValue(s.rewindMegabytes);
    m_rewindInterval->setValue(s.rewindInterval);
    for (const auto& [system, combo] : m_cores)
        combo->setCurrentIndex(std::max(0, combo->findData(s.coreChoices.value(systemKey(system)))));
}

void EmulationPage::applyTo(AppSettings& s) const
{
    s.speedMethod = static_cast<SpeedMethod>(m_method->currentData().toInt());
    s.fixedFrameRate = m_frameRate->value();
    s.maxFrameSkip = m_maxSkip->value();
    s.fastForwardSkip = m_fastForwardSkip->value();
    s.fixedFrameSkip = m_fixedSkip->value();
    s.rewindMegabytes = m_rewindMb->value();
    s.rewindInterval = m_rewindInterval->value();
    s.coreChoices.clear();
    for (const auto& [system, combo] : m_cores)
        if (!combo->currentData().toString().isEmpty())
            s.coreChoices[systemKey(system)] = combo->currentData().toString();
}
