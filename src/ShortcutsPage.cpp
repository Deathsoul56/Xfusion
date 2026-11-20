#include "ShortcutsPage.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "BindingIcons.h"

namespace {

constexpr int kSlots = ShortcutMap::kSlots;
constexpr int kResetColumn = kSlots;
constexpr int kClearColumn = kSlots + 1;
constexpr int kCapturePollMs = 16;
const QColor kConflict(230, 200, 40);

bool isModifierKey(int key)
{
    return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_Meta
        || key == Qt::Key_Super_L || key == Qt::Key_Super_R;
}

}

ShortcutsPage::ShortcutsPage(InputManager& input, const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_input(input)
    , m_map(settings.shortcuts)
    , m_table(new QTableWidget(this))
{
    auto* resetButton = new QToolButton(this);
    resetButton->setText("Reset to Default");
    resetButton->setPopupMode(QToolButton::InstantPopup);
    resetButton->setToolTip("Put the default binding of every action into one slot, leaving the other slots alone");
    auto* resetMenu = new QMenu(resetButton);
    for (int slot = 0; slot < kSlots; ++slot) {
        resetMenu->addAction(QString("Slot %1").arg(slot + 1), this, [this, slot] {
            endCapture();
            m_map.applyDefaultsToSlot(slot);
            refreshAll();
        });
    }
    resetButton->setMenu(resetMenu);
    auto* clearAll = new QPushButton("Clear All", this);
    connect(clearAll, &QPushButton::clicked, this, [this] {
        endCapture();
        m_map.clearAll();
        refreshAll();
    });

    auto* top = new QHBoxLayout;
    top->addWidget(resetButton);
    top->addWidget(clearAll);
    top->addStretch(1);

    const std::vector<ShortcutDef>& shortcuts = allShortcuts();
    m_table->setRowCount(static_cast<int>(shortcuts.size()));
    m_table->setColumnCount(kClearColumn + 1);
    QStringList headers;
    for (int slot = 1; slot <= kSlots; ++slot)
        headers << QString("Binding #%1").arg(slot);
    headers << "Reset" << "Clear";
    m_table->setHorizontalHeaderLabels(headers);
    QStringList labels;
    for (const ShortcutDef& def : shortcuts)
        labels << def.label;
    m_table->setVerticalHeaderLabels(labels);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->verticalHeader()->setFixedWidth(210);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int column = 0; column < kSlots; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kResetColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(kClearColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(kResetColumn, 60);
    m_table->setColumnWidth(kClearColumn, 60);

    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int slot = 0; slot < kSlots; ++slot)
            m_table->setItem(row, slot, new QTableWidgetItem);
        auto* reset = new QPushButton("\u21BA", m_table);
        reset->setToolTip("Reset this shortcut to its default binding");
        connect(reset, &QPushButton::clicked, this, [this, row] {
            endCapture();
            m_map.resetAction(allShortcuts()[static_cast<std::size_t>(row)].id);
            refreshAll();
        });
        m_table->setCellWidget(row, kResetColumn, reset);
        auto* clear = new QPushButton("\u2212", m_table);
        clear->setToolTip("Clear all the bindings of this shortcut");
        connect(clear, &QPushButton::clicked, this, [this, row] {
            endCapture();
            m_map.clearAction(allShortcuts()[static_cast<std::size_t>(row)].id);
            refreshAll();
        });
        m_table->setCellWidget(row, kClearColumn, clear);
    }
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column < kSlots)
            beginCapture(row, column);
    });
    connect(m_table, &QTableWidget::cellActivated, this, [this](int row, int column) {
        if (column < kSlots)
            beginCapture(row, column);
    });

    m_pollTimer.setInterval(kCapturePollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ShortcutsPage::pollJoystick);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_table, 1);
    refreshAll();
}

void ShortcutsPage::restoreDefaults()
{
    endCapture();
    m_map = ShortcutMap::defaults();
    refreshAll();
}

void ShortcutsPage::refreshCell(int row, int slot)
{
    QTableWidgetItem* item = m_table->item(row, slot);
    if (!item)
        return;
    const InputBinding& binding = m_map.bindings(allShortcuts()[static_cast<std::size_t>(row)].id)[slot];
    if (m_capture && m_capture->row == row && m_capture->slot == slot) {
        item->setText("...");
        item->setIcon({});
        return;
    }
    item->setText(binding.displayName());
    item->setIcon(bindingIcon(binding, palette().color(QPalette::Text)));
    const bool conflict = m_map.usesOf(binding) > 1;
    item->setBackground(conflict ? QBrush(kConflict) : QBrush());
    item->setForeground(conflict ? QBrush(Qt::black) : binding.isNone() ? QBrush(palette().color(QPalette::Disabled, QPalette::Text)) : QBrush(palette().color(QPalette::Text)));
    item->setToolTip(conflict ? "This input is used by more than one shortcut" : QString());
}

void ShortcutsPage::refreshAll()
{
    for (int row = 0; row < m_table->rowCount(); ++row)
        for (int slot = 0; slot < kSlots; ++slot)
            refreshCell(row, slot);
}

void ShortcutsPage::beginCapture(int row, int slot)
{
    endCapture();
    m_capture = Capture{row, slot};
    refreshCell(row, slot);
    m_input.beginCapture();
    grabKeyboard();
    m_pollTimer.start();
}

void ShortcutsPage::endCapture()
{
    if (!m_capture)
        return;
    m_pollTimer.stop();
    releaseKeyboard();
    m_capture.reset();
    refreshAll();
}

void ShortcutsPage::finishCapture(const InputBinding& binding)
{
    if (!m_capture)
        return;
    const Capture capture = *m_capture;
    m_map.set(allShortcuts()[static_cast<std::size_t>(capture.row)].id, capture.slot, binding);
    endCapture();
}

void ShortcutsPage::keyPressEvent(QKeyEvent* event)
{
    if (!m_capture) {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    // A bare modifier waits for the key it modifies, so that Ctrl+G can be captured.
    if (event->isAutoRepeat() || event->key() == Qt::Key_unknown || isModifierKey(event->key()))
        return;
    if (event->key() == Qt::Key_Escape) {
        finishCapture({});
        return;
    }
    finishCapture(InputBinding::key(event->key(), 0, static_cast<int>(event->modifiers())));
}

void ShortcutsPage::hideEvent(QHideEvent* event)
{
    endCapture();
    QWidget::hideEvent(event);
}

void ShortcutsPage::pollJoystick()
{
    if (const auto binding = m_input.captureJoystick())
        finishCapture(*binding);
}
