#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ControllersPage.h"

SettingsDialog::SettingsDialog(InputManager& input, const InputConfig& config, InputGroup initialGroup, QWidget* parent)
    : QDialog(parent)
    , m_controllers(new ControllersPage(input, config, initialGroup, this))
{
    setWindowTitle("Settings");
    resize(960, 720);

    auto* pages = new QListWidget(this);
    pages->setFixedWidth(190);
    pages->setIconSize({24, 24});
    QFont font = pages->font();
    font.setPointSize(font.pointSize() + 1);
    pages->setFont(font);
    pages->addItem("Controllers");
    pages->setCurrentRow(0);

    auto* stack = new QStackedWidget(this);
    stack->addWidget(m_controllers);
    connect(pages, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults
                                             | QDialogButtonBox::Help,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, m_controllers, &ControllersPage::restoreDefaults);
    connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "Controllers",
                                 "Pick the platform group, click a binding cell and press a key or a controller input.\n\n"
                                 "Genesis, Sega CD and 32X share one set of bindings; Master System, Game Gear and SG-1000 share another.\n"
                                 "A button or key can only be used once per group: binding it again moves it.\n"
                                 "Right-click a cell to clear it. Auto-Assign fills the controller column from a connected gamepad.");
    });

    auto* body = new QHBoxLayout;
    body->addWidget(pages);
    body->addWidget(stack, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);
}

InputConfig SettingsDialog::inputConfig() const
{
    return m_controllers->config();
}
