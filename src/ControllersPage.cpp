#include "ControllersPage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ControllerDiagram.h"

namespace {

constexpr int kCaptureTimeoutMs = 8000;
constexpr int kCapturePollMs = 16;
constexpr int kFirstBindingColumn = 1;
constexpr int kClearColumn = kFirstBindingColumn + InputConfig::kSlots;

}

ControllersPage::ControllersPage(InputManager& input, const InputConfig& config, InputGroup initialGroup, QWidget* parent)
    : QWidget(parent)
    , m_input(input)
    , m_config(config)
    , m_platform(new QComboBox(this))
    , m_status(new QLabel(this))
    , m_diagram(new ControllerDiagram(this))
    , m_table(new QTableWidget(this))
{
    for (const InputGroupDef& group : allInputGroups()) {
        m_platform->addItem(inputGroupTitle(group.id), static_cast<int>(group.id));
        if (group.id == initialGroup)
            m_platform->setCurrentIndex(m_platform->count() - 1);
    }

    auto* autoAssignButton = new QPushButton("Auto-Assign", this);
    autoAssignButton->setToolTip("Fill the controller column from the standard layout of the connected gamepad");

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel("Platform:", this));
    top->addWidget(m_platform, 1);
    top->addWidget(autoAssignButton);

    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setMinimumHeight(m_status->fontMetrics().height() + 6);

    m_table->setColumnCount(kClearColumn + 1);
    QStringList headers = {""};
    for (int slot = 1; slot <= InputConfig::kSlots; ++slot)
        headers << QString("Binding #%1").arg(slot);
    headers << "Clear";
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
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
    layout->addLayout(top);
    layout->addWidget(m_status);
    layout->addWidget(m_diagram, 1);
    layout->addWidget(m_table, 2);

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
    connect(autoAssignButton, &QPushButton::clicked, this, &ControllersPage::autoAssign);
    const auto startCapture = [this](int row, int column) {
        if (column >= kFirstBindingColumn && column < kClearColumn)
            beginCapture(row, column - kFirstBindingColumn);
    };
    connect(m_table, &QTableWidget::cellClicked, this, startCapture);
    connect(m_table, &QTableWidget::cellActivated, this, startCapture);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this] { highlightCurrentRow(); });
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const QModelIndex index = m_table->indexAt(position);
        if (!index.isValid() || index.column() < kFirstBindingColumn || index.column() >= kClearColumn)
            return;
        QMenu menu(this);
        QAction* clear = menu.addAction("Clear this binding");
        if (menu.exec(m_table->viewport()->mapToGlobal(position)) == clear)
            clearCell(index.row(), index.column() - kFirstBindingColumn);
    });

    rebuildTable();
}

InputGroup ControllersPage::currentGroup() const
{
    return static_cast<InputGroup>(m_platform->currentData().toInt());
}

void ControllersPage::rebuildTable()
{
    const InputGroupDef& group = inputGroupDef(currentGroup());
    m_diagram->setGroup(group.id);
    m_rowButtons.clear();
    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(group.actions.size()));

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const InputAction& action = group.actions[static_cast<std::size_t>(row)];
        m_rowButtons.push_back(action.button);

        auto* label = new QTableWidgetItem(action.label);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        label->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(row, 0, label);

        for (int slot = 0; slot < InputConfig::kSlots; ++slot)
            m_table->setItem(row, kFirstBindingColumn + slot, new QTableWidgetItem);

        auto* clear = new QPushButton("\u2212", m_table);
        clear->setToolTip("Clear all bindings of this button");
        connect(clear, &QPushButton::clicked, this, [this, row] { clearRow(row); });
        m_table->setCellWidget(row, kClearColumn, clear);

        refreshRow(row);
    }
    m_table->setCurrentCell(0, kFirstBindingColumn);
    highlightCurrentRow();
}

void ControllersPage::refreshCell(int row, int slot)
{
    QTableWidgetItem* item = m_table->item(row, kFirstBindingColumn + slot);
    if (!item)
        return;
    const InputBinding& binding = m_config.bindings(currentGroup(), m_rowButtons[static_cast<std::size_t>(row)])[slot];
    item->setText(binding.displayName());
    item->setForeground(binding.isNone() ? palette().color(QPalette::Disabled, QPalette::Text) : palette().color(QPalette::Text));
}

void ControllersPage::refreshRow(int row)
{
    for (int slot = 0; slot < InputConfig::kSlots; ++slot)
        refreshCell(row, slot);
}

void ControllersPage::highlightCurrentRow()
{
    const int row = m_table->currentRow();
    if (row >= 0 && row < static_cast<int>(m_rowButtons.size()))
        m_diagram->setHighlight(m_rowButtons[static_cast<std::size_t>(row)]);
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
    const auto displaced = m_config.assign(currentGroup(), capture.button, capture.slot, binding);
    endCapture();

    QString message = QString("Bound %1").arg(binding.displayName());
    if (displaced) {
        for (const InputAction& action : inputGroupDef(currentGroup()).actions)
            if (action.button == *displaced)
                message += QString(" (moved from %1)").arg(action.label);
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

void ControllersPage::autoAssign()
{
    endCapture();
    QString message;
    m_input.autoAssign(currentGroup(), m_config, &message);
    for (int row = 0; row < m_table->rowCount(); ++row)
        refreshRow(row);
    showStatus(message);
}

void ControllersPage::clearRow(int row)
{
    endCapture();
    m_config.clearAction(currentGroup(), m_rowButtons[static_cast<std::size_t>(row)]);
    refreshRow(row);
    showStatus({});
}

void ControllersPage::clearCell(int row, int slot)
{
    endCapture();
    m_config.clearSlot(currentGroup(), m_rowButtons[static_cast<std::size_t>(row)], slot);
    refreshCell(row, slot);
    showStatus({});
}

void ControllersPage::restoreDefaults()
{
    endCapture();
    m_config.resetGroup(currentGroup());
    for (int row = 0; row < m_table->rowCount(); ++row)
        refreshRow(row);
    showStatus("Defaults restored");
}
