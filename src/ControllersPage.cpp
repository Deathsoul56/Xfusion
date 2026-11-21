#include "ControllersPage.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

#include "BindingIcons.h"
#include "Icons.h"
#include "ControllerDiagram.h"

namespace {

constexpr int kCaptureTimeoutMs = 8000;
constexpr int kCapturePollMs = 16;
constexpr int kLivePollMs = 30;
constexpr int kDeviceRefreshTicks = 33; // about once a second at kLivePollMs
constexpr int kFirstBindingColumn = 1;
constexpr int kClearColumn = kFirstBindingColumn + InputConfig::kSlots;

}

ControllersPage::ControllersPage(InputManager& input, const InputConfig& config, InputGroup initialGroup, QWidget* parent)
    : QWidget(parent)
    , m_input(input)
    , m_config(config)
    , m_platform(new QComboBox(this))
    , m_ports(new QComboBox(this))
    , m_player(new QComboBox(this))
    , m_autoAssignButton(new QToolButton(this))
    , m_autoAssignMenu(new QMenu(this))
    , m_raw(new QLabel(this))
    , m_device(new QComboBox(this))
    , m_padRow(new QWidget(this))
    , m_padType(new QComboBox(this))
    , m_status(new QLabel(this))
    , m_diagram(new ControllerDiagram(this))
    , m_table(new QTableWidget(this))
{
    for (const InputGroupDef& group : allInputGroups()) {
        m_platform->addItem(inputGroupTitle(group.id), static_cast<int>(group.id));
        if (group.id == initialGroup)
            m_platform->setCurrentIndex(m_platform->count() - 1);
    }
    m_ports->addItem("One Controller", static_cast<int>(InputConfig::PortMode::OneController));
    m_ports->addItem("Two Controllers", static_cast<int>(InputConfig::PortMode::TwoControllers));
    m_ports->setToolTip("With one controller the second port is empty for the game.");
    for (int player = 0; player < InputConfig::kPlayers; ++player)
        m_player->addItem(QString("SEGA Controller %1").arg(player + 1), player);
    m_device->setToolTip("The gamepad this player uses. Automatic gives each player the next connected gamepad that nobody "
                         "picked by name. The keyboard is shared.");

    // The Edit and Auto-Assign menus work on the controller shown in the Set box.
    auto* editButton = new QToolButton(this);
    editButton->setText("Edit");
    editButton->setPopupMode(QToolButton::InstantPopup);
    editButton->setToolTip("Clear groups of bindings");
    auto* editMenu = new QMenu(editButton);
    editMenu->addAction("Clear Current Controller", this, [this] {
        endCapture();
        m_config.clearPlayer(currentGroup(), currentPlayer());
        rebuildTable();
        showStatus("Cleared this controller");
    });
    editMenu->addAction("Clear All Controllers", this, [this] {
        endCapture();
        for (int player = 0; player < InputConfig::kPlayers; ++player)
            m_config.clearPlayer(currentGroup(), player);
        rebuildTable();
        showStatus("Cleared all controllers");
    });
    editButton->setMenu(editMenu);
    m_autoAssignButton->setText("Auto-Assign");
    m_autoAssignButton->setPopupMode(QToolButton::InstantPopup);
    m_autoAssignButton->setToolTip("Fill one binding column from the keyboard defaults or from a connected gamepad");
    m_autoAssignButton->setMenu(m_autoAssignMenu);
    m_raw->setStyleSheet("color: gray; font-size: 9pt;");

    // The labels share one width so that every box starts at the same distance from the edge.
    std::vector<QLabel*> labels;
    const auto rowLabel = [&labels](const QString& text, QWidget* parent) {
        auto* label = new QLabel(text, parent);
        labels.push_back(label);
        return label;
    };

    // The platform group is the parent: ports, sets and bindings below all belong to it.
    auto* platformRow = new QHBoxLayout;
    platformRow->addWidget(rowLabel("Platform:", this));
    platformRow->addWidget(m_platform, 1);

    auto* portsRow = new QHBoxLayout;
    portsRow->addWidget(rowLabel("Controller Ports:", this));
    portsRow->addWidget(m_ports, 1);

    auto* top = new QHBoxLayout;
    top->addWidget(rowLabel("Set:", this));
    top->addWidget(m_player, 1);
    top->addWidget(editButton);
    top->addWidget(m_autoAssignButton);

    auto* deviceRow = new QHBoxLayout;
    deviceRow->addWidget(rowLabel("Gamepad:", this));
    deviceRow->addWidget(m_device, 1);

    m_padType->addItem("6 button pad", static_cast<int>(PadType::SixButton));
    m_padType->addItem("3 button pad", static_cast<int>(PadType::ThreeButton));
    m_padType->setToolTip("The pad the game sees, for both players. Some games misbehave with the 6 button pad; X, Y, Z and Mode only exist on it.");
    auto* padLayout = new QHBoxLayout(m_padRow);
    padLayout->setContentsMargins(0, 0, 0, 0);
    padLayout->addWidget(rowLabel("Pad type:", m_padRow));
    padLayout->addWidget(m_padType, 1);
    int labelWidth = 0;
    for (const QLabel* label : labels)
        labelWidth = std::max(labelWidth, label->sizeHint().width());
    for (QLabel* label : labels)
        label->setMinimumWidth(labelWidth);

    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setMinimumHeight(m_status->fontMetrics().height() + 6);

    m_table->setColumnCount(kClearColumn + 1);
    QStringList headers = {""};
    for (int slot = 1; slot <= InputConfig::kSlots; ++slot)
        headers << QString("Binding #%1").arg(slot);
    headers << "Clear";
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int column = kFirstBindingColumn; column < kClearColumn; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kClearColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(kClearColumn, 70);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(platformRow);
    layout->addLayout(portsRow);
    layout->addLayout(top);
    layout->addLayout(deviceRow);
    layout->addWidget(m_padRow);
    layout->addWidget(m_status);
    layout->addWidget(m_diagram, 3);
    layout->addWidget(m_raw);
    layout->addWidget(m_table, 4);

    m_pollTimer.setInterval(kCapturePollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllersPage::pollController);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(kCaptureTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        endCapture();
        showStatus("Timed out; nothing was bound");
    });

    connect(m_platform, &QComboBox::currentIndexChanged, this, [this] {
        endCapture();
        rebuildTable();
        showStatus({});
    });
    connect(m_player, &QComboBox::currentIndexChanged, this, [this] {
        endCapture();
        refreshDevices(true);
        rebuildTable();
        showStatus({});
    });
    connect(m_device, &QComboBox::activated, this, [this] {
        m_config.setDevice(currentPlayer(), m_device->currentData().toString());
        showStatus({});
    });
    connect(m_ports, &QComboBox::activated, this, [this] {
        m_config.setPortMode(static_cast<InputConfig::PortMode>(m_ports->currentData().toInt()));
        showStatus({});
    });
    connect(m_padType, &QComboBox::currentIndexChanged, this, [this] {
        endCapture();
        m_config.setPadType(static_cast<PadType>(m_padType->currentData().toInt()));
        rebuildTable();
        showStatus({});
    });
    connect(m_diagram, &ControllerDiagram::buttonClicked, this, [this](PadButton button) {
        const auto it = std::find(m_rowButtons.begin(), m_rowButtons.end(), button);
        if (it != m_rowButtons.end())
            m_table->setCurrentCell(static_cast<int>(it - m_rowButtons.begin()), kFirstBindingColumn);
    });
    const auto startCapture = [this](int row, int column) {
        if (column >= kFirstBindingColumn && column < kClearColumn)
            beginCapture(row, column - kFirstBindingColumn);
    };
    connect(m_table, &QTableWidget::cellClicked, this, startCapture);
    connect(m_table, &QTableWidget::cellActivated, this, startCapture);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const QModelIndex index = m_table->indexAt(position);
        if (!index.isValid() || index.column() < kFirstBindingColumn || index.column() >= kClearColumn)
            return;
        QMenu menu(this);
        QAction* clear = menu.addAction("Clear this binding");
        if (menu.exec(m_table->viewport()->mapToGlobal(position)) == clear)
            clearCell(index.row(), index.column() - kFirstBindingColumn);
    });

    m_liveTimer.setInterval(kLivePollMs);
    connect(&m_liveTimer, &QTimer::timeout, this, &ControllersPage::updateLiveButtons);
    m_liveTimer.start();

    m_ports->setCurrentIndex(m_ports->findData(static_cast<int>(m_config.portMode())));
    refreshDevices(true);
    rebuildAutoAssignMenu();
    rebuildTable();
}

InputGroup ControllersPage::currentGroup() const
{
    return static_cast<InputGroup>(m_platform->currentData().toInt());
}

int ControllersPage::currentPlayer() const
{
    return m_player->currentData().toInt();
}

// Lists the connected gamepads. One that was picked but is unplugged stays in the list so the choice is not
// lost silently.
void ControllersPage::refreshDevices(bool force)
{
    const QStringList names = m_input.controllerNames();
    if (!force && (names == m_knownDevices || m_device->view()->isVisible()))
        return;
    m_knownDevices = names;
    rebuildAutoAssignMenu();

    const QString chosen = m_config.device(currentPlayer());
    const QSignalBlocker blocker(m_device);
    m_device->clear();
    m_device->addItem("Automatic", QString());
    for (const QString& name : names)
        if (m_device->findData(name) < 0)
            m_device->addItem(name, name);
    if (!chosen.isEmpty() && m_device->findData(chosen) < 0)
        m_device->addItem(QString("%1 (not connected)").arg(chosen), chosen);
    m_device->setCurrentIndex(qMax(0, m_device->findData(chosen)));
}

void ControllersPage::syncPadTypeCombo()
{
    const QSignalBlocker blocker(m_padType);
    m_padType->setCurrentIndex(m_padType->findData(static_cast<int>(m_config.padType())));
}

void ControllersPage::rebuildTable()
{
    const InputGroupDef& group = inputGroupDef(currentGroup());
    m_padRow->setVisible(group.selectablePadType);
    syncPadTypeCombo();
    m_diagram->setGroup(group.id);
    m_diagram->setPadType(m_config.padType());
    m_rowButtons.clear();
    m_table->clearContents();

    std::vector<const InputAction*> actions;
    for (const InputAction& action : group.actions)
        if (!group.selectablePadType || action.availableWith(m_config.padType()))
            actions.push_back(&action);
    m_table->setRowCount(static_cast<int>(actions.size()));

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const InputAction& action = *actions[static_cast<std::size_t>(row)];
        m_rowButtons.push_back(action.button);

        auto* label = new QTableWidgetItem(action.label);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        label->setFlags(Qt::ItemIsEnabled);
        label->setIcon(padButtonIcon(action.button, palette().color(QPalette::Text)));
        m_table->setItem(row, 0, label);

        for (int slot = 0; slot < InputConfig::kSlots; ++slot)
            m_table->setItem(row, kFirstBindingColumn + slot, new QTableWidgetItem);

        auto* clear = new QPushButton("\u2212", m_table);
        clear->setObjectName("clearBindingButton");
        clear->setToolTip("Clear all bindings of this button");
        connect(clear, &QPushButton::clicked, this, [this, row] { clearRow(row); });
        m_table->setCellWidget(row, kClearColumn, clear);

        refreshRow(row);
    }
    m_table->setCurrentCell(0, kFirstBindingColumn);
}

void ControllersPage::refreshCell(int row, int slot)
{
    QTableWidgetItem* item = m_table->item(row, kFirstBindingColumn + slot);
    if (!item)
        return;
    const InputBinding& binding = m_config.bindings(currentGroup(), currentPlayer(), m_rowButtons[static_cast<std::size_t>(row)])[slot];
    item->setText(binding.displayName());
    item->setIcon(bindingIcon(binding, palette().color(QPalette::Text)));
    item->setForeground(binding.isNone() ? palette().color(QPalette::Disabled, QPalette::Text) : palette().color(QPalette::Text));
}

void ControllersPage::refreshRow(int row)
{
    for (int slot = 0; slot < InputConfig::kSlots; ++slot)
        refreshCell(row, slot);
}

// The pad picture shows what is pressed right now, using the bindings being edited. While a binding is being
// captured it shows the button that will receive it instead.
void ControllersPage::updateLiveButtons()
{
    InputManager::PadState pressed{};
    if (m_capture)
        pressed[static_cast<std::size_t>(m_capture->button)] = true;
    else if (isActiveWindow())
        pressed = m_input.pressedWith(currentGroup(), currentPlayer(), m_config);
    m_diagram->setPressed(pressed);
    m_raw->setText(m_capture ? m_raw->text() : (m_input.debugRawState().isEmpty() ? QString() : "raw SDL: " + m_input.debugRawState()));

    if (++m_liveTicks >= kDeviceRefreshTicks) {
        m_liveTicks = 0;
        refreshDevices(false);
    }
}

void ControllersPage::showStatus(const QString& text)
{
    m_status->setText(text);
}

void ControllersPage::beginCapture(int row, int slot)
{
    endCapture();
    m_capture = Capture{m_rowButtons[static_cast<std::size_t>(row)], slot};
    if (QTableWidgetItem* item = m_table->item(row, kFirstBindingColumn + slot))
        item->setText("Press a key or controller input...");
    m_input.beginCapture();
    grabKeyboard();
    m_pollTimer.start();
    m_timeout.start();
    showStatus("Esc cancels, right-click clears");
}

void ControllersPage::endCapture()
{
    if (!m_capture)
        return;
    m_pollTimer.stop();
    m_timeout.stop();
    releaseKeyboard();
    m_capture.reset();
    for (int row = 0; row < m_table->rowCount(); ++row)
        refreshRow(row);
}

void ControllersPage::finishCapture(const InputBinding& binding)
{
    if (!m_capture)
        return;
    const Capture capture = *m_capture;
    const auto displaced = m_config.assign(currentGroup(), currentPlayer(), capture.button, capture.slot, binding);
    endCapture();

    QString message = QString("Bound %1").arg(binding.displayName());
    if (displaced) {
        for (const InputAction& action : inputGroupDef(currentGroup()).actions) {
            if (action.button != displaced->button)
                continue;
            message += displaced->player == currentPlayer()
                ? QString(" (moved from %1)").arg(action.label)
                : QString(" (moved from player %1 %2)").arg(displaced->player + 1).arg(action.label);
        }
    }
    showStatus(message);
}

void ControllersPage::keyPressEvent(QKeyEvent* event)
{
    if (!m_capture) {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    if (event->isAutoRepeat() || event->key() == Qt::Key_unknown)
        return;
    if (event->key() == Qt::Key_Escape) {
        endCapture();
        showStatus("Cancelled");
        return;
    }
#ifdef Q_OS_WIN
    const int virtualKey = static_cast<int>(event->nativeVirtualKey());
#else
    const int virtualKey = 0;
#endif
    finishCapture(InputBinding::key(event->key(), virtualKey));
}

void ControllersPage::pollController()
{
    if (const auto binding = m_input.captureJoystick())
        finishCapture(*binding);
}

// One submenu per binding column; each offers the default keyboard and every connected gamepad.
void ControllersPage::rebuildAutoAssignMenu()
{
    m_autoAssignMenu->clear();
    for (int slot = 0; slot < InputConfig::kSlots; ++slot) {
        QMenu* submenu = m_autoAssignMenu->addMenu(QString("Binding Set #%1").arg(slot + 1));
        submenu->addAction("Default Keyboard", this, [this, slot] { autoAssign(slot, QString()); });
        for (const QString& name : m_knownDevices) {
            if (!name.isEmpty())
                submenu->addAction(name, this, [this, slot, name] { autoAssign(slot, name); });
        }
    }
}

void ControllersPage::autoAssign(int slot, const QString& deviceName)
{
    endCapture();
    QString message;
    if (deviceName.isEmpty()) {
        m_config.applyDefaultKeyboard(currentGroup(), currentPlayer(), slot);
        message = QString("Assigned the default keyboard to binding #%1").arg(slot + 1);
    } else {
        m_input.autoAssign(currentGroup(), currentPlayer(), slot, deviceName, m_config, &message);
    }
    for (int row = 0; row < m_table->rowCount(); ++row)
        refreshRow(row);
    showStatus(message);
}

void ControllersPage::clearRow(int row)
{
    endCapture();
    m_config.clearAction(currentGroup(), currentPlayer(), m_rowButtons[static_cast<std::size_t>(row)]);
    refreshRow(row);
    showStatus({});
}

void ControllersPage::clearCell(int row, int slot)
{
    endCapture();
    m_config.clearSlot(currentGroup(), currentPlayer(), m_rowButtons[static_cast<std::size_t>(row)], slot);
    refreshCell(row, slot);
    showStatus({});
}

void ControllersPage::restoreDefaults()
{
    endCapture();
    m_config.resetPlayer(currentGroup(), currentPlayer());
    rebuildTable();
    showStatus(QString("Player %1 defaults restored").arg(currentPlayer() + 1));
}
