#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QScreen>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "AchievementsPage.h"
#include "ControllersPage.h"
#include "DisplayPage.h"
#include "EmulationPage.h"
#include "FilesPage.h"
#include "GeneralPage.h"
#include "Icons.h"
#include "ShortcutsPage.h"
#include "SoundPage.h"

namespace {

// Pages that can be taller than a small window get a scroll bar instead of squeezing their controls.
QWidget* scrolled(QWidget* page, QWidget* parent)
{
    auto* area = new QScrollArea(parent);
    area->setWidget(page);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    return area;
}

}

SettingsDialog::SettingsDialog(InputManager& input, const InputConfig& config, const AppSettings& settings, InputGroup initialGroup,
                               Page initialPage, Achievements* achievements, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_pages(new QListWidget(this))
    , m_stack(new QStackedWidget(this))
    , m_general(new GeneralPage(settings, this))
    , m_display(new DisplayPage(settings, this))
    , m_sound(new SoundPage(settings, this))
    , m_emulation(new EmulationPage(settings, this))
    , m_controllers(new ControllersPage(input, config, initialGroup, this))
    , m_shortcuts(new ShortcutsPage(input, settings, this))
    , m_files(new FilesPage(settings, this))
    , m_achievementsPage(new AchievementsPage(settings, achievements, this))
{
    setWindowTitle("Settings");
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    resize(1000, qMin(860, screen.height() - 80));

    m_pages->setFixedWidth(190);
    QFont font = m_pages->font();
    font.setPointSize(font.pointSize() + 1);
    m_pages->setFont(font);
    // Same order as the stack below and as the Page enum.
    m_pages->setObjectName("panelList");
    const char* const pageNames[] = {"General", "Display", "Sound", "Emulation", "Controllers", "Shortcuts", "Files", "Achievements"};
    const char* const pageIcons[] = {"settings", "display", "sound", "emulation", "joypad", "keyboard", "folders", "achievements"};
    for (int page = 0; page < 8; ++page)
        m_pages->addItem(new QListWidgetItem(themedIcon(pageIcons[page]), pageNames[page]));

    m_stack->addWidget(scrolled(m_general, this));
    m_stack->addWidget(scrolled(m_display, this));
    m_stack->addWidget(scrolled(m_sound, this));
    m_stack->addWidget(scrolled(m_emulation, this));
    m_stack->addWidget(m_controllers);
    m_stack->addWidget(m_shortcuts);
    m_stack->addWidget(scrolled(m_files, this));
    m_stack->addWidget(scrolled(m_achievementsPage, this));
    connect(m_pages, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_pages->setCurrentRow(initialPage);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults
                                             | QDialogButtonBox::Help,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &SettingsDialog::restoreCurrentPageDefaults);
    connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, this, &SettingsDialog::showCurrentPageHelp);

    auto* body = new QHBoxLayout;
    body->addWidget(m_pages);
    body->addWidget(m_stack, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);
}

InputConfig SettingsDialog::inputConfig() const
{
    return m_controllers->config();
}

AppSettings SettingsDialog::appSettings() const
{
    AppSettings settings = m_settings;
    m_general->applyTo(settings);
    m_display->applyTo(settings);
    m_sound->applyTo(settings);
    m_emulation->applyTo(settings);
    m_shortcuts->applyTo(settings);
    m_files->applyTo(settings);
    m_achievementsPage->applyTo(settings);
    return settings;
}

void SettingsDialog::restoreCurrentPageDefaults()
{
    switch (static_cast<Page>(m_stack->currentIndex())) {
    case General: m_general->restoreDefaults(); break;
    case Display: m_display->restoreDefaults(); break;
    case Sound: m_sound->restoreDefaults(); break;
    case Emulation: m_emulation->restoreDefaults(); break;
    case Controllers: m_controllers->restoreDefaults(); break;
    case Shortcuts: m_shortcuts->restoreDefaults(); break;
    case Files: m_files->restoreDefaults(); break;
    case RetroAchievements: m_achievementsPage->restoreDefaults(); break;
    }
}

void SettingsDialog::showCurrentPageHelp()
{
    static const char* const texts[] = {
        "Window behaviour, the theme, the on-screen overlays and how often the battery RAM is written. Hover a setting for details.",
        "Scaling, aspect ratio, filters and on-screen messages. These settings apply to every system.",
        "Volume, output device and buffer. Volume and mute apply to every system.",
        "Speed control and rewind. Sound Synchronization waits on the audio; Timer and None do not need sound.",
        "Pick the platform group and the controller set, click a binding cell and press a key or a controller input.\n\n"
        "Genesis, Sega CD and 32X share one set of bindings; Master System, Game Gear and SG-1000 share another.\n"
        "A key can only drive one action of the group, for either controller; a gamepad button only one action of the same controller. "
        "Binding it again moves it. Right-click a cell to clear it. Turbo rows auto-fire the button they name.\n"
        "Each controller reads one gamepad: Automatic gives them the connected gamepads in the order they were plugged in, or pick one by name.\n"
        "With one controller the second port is empty for the game.",
        ("Click a binding cell and press a key (with its modifiers) or a gamepad input. Esc clears the cell. A binding used by "
         "more than one shortcut is highlighted but still works. Reset to Default writes the defaults into one binding column."),
        "Where saves, states, screenshots and BIOS files are kept. Changes apply the next time a game opens.",
        "Sign in with a RetroAchievements account to track the achievements of the game you play. Only casual mode is offered; "
        "games with a patch applied have no achievements.",
    };
    const int index = m_stack->currentIndex();
    QMessageBox::information(this, m_pages->item(index)->text(), texts[index]);
}
