#include "LibraryPage.h"

#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

#include "AppPaths.h"

namespace {

QString sizeText(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
    return QString("%1 KB").arg((bytes + 1023) / 1024);
}

QString playtimeText(qint64 ms)
{
    if (ms <= 0)
        return {};
    const qint64 minutes = ms / 60000;
    if (minutes >= 60)
        return QString("%1h %2m").arg(minutes / 60).arg(minutes % 60, 2, 10, QChar('0'));
    return minutes > 0 ? QString("%1m").arg(minutes) : QString("<1m");
}

QString lastPlayedText(qint64 ms)
{
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd hh:mm") : QString("Never");
}

// A table that Enter opens a game from, as a double-click does, and whose last column takes the width that is left.
class GameTable : public QTableView {
public:
    std::function<void(const QModelIndex&)> activate;

    void fitLastColumn()
    {
        if (isColumnHidden(LibraryModel::Location))
            return;
        int used = 0;
        for (int column = 0; column < LibraryModel::Location; ++column)
            if (!isColumnHidden(column))
                used += columnWidth(column);
        setColumnWidth(LibraryModel::Location, std::max(160, viewport()->width() - used));
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QTableView::resizeEvent(event);
        fitLastColumn();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && activate && currentIndex().isValid()) {
            activate(currentIndex());
            return;
        }
        QTableView::keyPressEvent(event);
    }
};

}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    const LibraryEntry* entry = entryAt(index.row());
    if (!entry || index.column() < 0 || index.column() >= ColumnCount)
        return {};
    const LibraryStats stats = m_stats.value(entry->id);
    if (role == PathRole)
        return entry->path;
    if (role == SystemRole)
        return static_cast<int>(entry->system);
    if (role == SortRole) {
        switch (index.column()) {
        case Name: return entry->name.toLower();
        case SystemColumn: return systemDef(entry->system).folder;
        case RegionColumn: return entry->region;
        case Size: return entry->size;
        case TimePlayed: return stats.playtimeMs;
        case LastPlayed: return stats.lastPlayedMs;
        default: return QDir::toNativeSeparators(entry->path).toLower();
        }
    }
    if (role == Qt::ToolTipRole) {
        QString text = QDir::toNativeSeparators(entry->path);
        if (!entry->title.isEmpty() && entry->title.compare(entry->name, Qt::CaseInsensitive) != 0)
            text = entry->title + "\n" + text;
        if (!entry->problem.isEmpty())
            text += "\nProblem: " + entry->problem;
        for (const QString& other : entry->duplicates)
            text += "\nAlso found at " + QDir::toNativeSeparators(other);
        return text;
    }
    if (role == Qt::ForegroundRole && !entry->problem.isEmpty() && index.column() == Name)
        return QColor(Qt::darkYellow);
    if (role == Qt::TextAlignmentRole && (index.column() == Size || index.column() == TimePlayed))
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case Name: return entry->name;
    case SystemColumn: return systemDef(entry->system).folder;
    case RegionColumn: return entry->region;
    case Size: return sizeText(entry->size);
    case TimePlayed: return playtimeText(stats.playtimeMs);
    case LastPlayed: return lastPlayedText(stats.lastPlayedMs);
    default: return QDir::toNativeSeparators(QFileInfo(entry->path).absolutePath());
    }
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* const titles[] = {"Name", "System", "Region", "Size", "Time Played", "Last Played", "Folder"};
    return section >= 0 && section < ColumnCount ? QVariant(titles[section]) : QVariant();
}

void LibraryModel::setEntries(std::vector<LibraryEntry> entries, const QMap<QString, LibraryStats>& stats)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_stats = stats;
    endResetModel();
}

void LibraryModel::setStats(const QMap<QString, LibraryStats>& stats)
{
    m_stats = stats;
    if (!m_entries.empty())
        emit dataChanged(index(0, TimePlayed), index(int(m_entries.size()) - 1, LastPlayed));
}

LibraryProxy::LibraryProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(LibraryModel::SortRole);
}

void LibraryProxy::setFilter(const QString& text, int system)
{
    beginFilterChange();
    m_text = text.trimmed();
    m_system = system;
    endFilterChange();
}

bool LibraryProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    const QModelIndex name = sourceModel()->index(row, LibraryModel::Name, parent);
    if (m_system >= 0 && sourceModel()->data(name, LibraryModel::SystemRole).toInt() != m_system)
        return false;
    return m_text.isEmpty() || sourceModel()->data(name, Qt::DisplayRole).toString().contains(m_text, Qt::CaseInsensitive);
}

LibraryPage::LibraryPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new LibraryModel(this))
    , m_proxy(new LibraryProxy(this))
    , m_title(new QLabel("Library", this))
    , m_search(new QLineEdit(this))
    , m_system(new QComboBox(this))
    , m_stack(new QStackedWidget(this))
    , m_emptyHint(new QLabel(this))
{
    setObjectName("libraryPage");
    setAttribute(Qt::WA_StyledBackground, true);
    m_data = LibraryData::load(AppPaths::libraryPath(), &m_loadError);
    m_proxy->setSourceModel(m_model);

    auto* header = new QWidget(this);
    header->setObjectName("libraryHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);
    m_title->setObjectName("libraryTitle");
    m_search->setObjectName("librarySearch");
    m_search->setPlaceholderText("Search...");
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(280);
    m_system->addItem("All systems", -1);
    for (const SystemDef& def : allSystems())
        m_system->addItem(def.folder, static_cast<int>(def.id));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->addWidget(m_title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_system);
    headerLayout->addWidget(m_search);

    auto* empty = new QWidget(m_stack);
    auto* drop = new QWidget(empty);
    drop->setObjectName("libraryDropZone");
    drop->setAttribute(Qt::WA_StyledBackground, true);
    drop->setFixedSize(460, 220);
    auto* emptyTitle = new QLabel("No games found", drop);
    emptyTitle->setObjectName("libraryEmptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyHint->setObjectName("libraryEmptyHint");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    auto* add = new QPushButton("Add Game Folder...", drop);
    connect(add, &QPushButton::clicked, this, &LibraryPage::chooseFolder);
    auto* dropLayout = new QVBoxLayout(drop);
    dropLayout->setContentsMargins(24, 24, 24, 24);
    dropLayout->setSpacing(8);
    dropLayout->addWidget(emptyTitle);
    dropLayout->addWidget(m_emptyHint, 1);
    dropLayout->addWidget(add, 0, Qt::AlignCenter);
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->addWidget(drop, 0, Qt::AlignCenter);

    auto* table = new GameTable;
    m_table = table;
    table->activate = [this](const QModelIndex& index) { activate(index); };
    m_table->setObjectName("gameTable");
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setTextElideMode(Qt::ElideMiddle);
    m_table->setWordWrap(false); // a path has no spaces to break at, and the rows have a fixed height
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    QHeaderView* columns = m_table->horizontalHeader();
    columns->setObjectName("gameHeader");
    columns->setSectionsClickable(true);
    columns->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    columns->setSectionResizeMode(QHeaderView::Interactive);
    columns->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setColumnWidth(LibraryModel::Name, 250);
    m_table->setColumnWidth(LibraryModel::SystemColumn, 110);
    m_table->setColumnWidth(LibraryModel::RegionColumn, 90);
    m_table->setColumnWidth(LibraryModel::Size, 80);
    m_table->setColumnWidth(LibraryModel::TimePlayed, 100);
    m_table->setColumnWidth(LibraryModel::LastPlayed, 140);
    m_table->sortByColumn(LibraryModel::Name, Qt::AscendingOrder);

    m_stack->addWidget(empty);
    m_stack->addWidget(m_table);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(m_stack, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this] { updateFilter(); });
    connect(m_system, &QComboBox::currentIndexChanged, this, [this] { updateFilter(); });
    connect(m_table, &QTableView::doubleClicked, this, &LibraryPage::activate);
    connect(m_table, &QTableView::customContextMenuRequested, this, &LibraryPage::showContextMenu);
    connect(columns, &QHeaderView::customContextMenuRequested, this, [this, columns](const QPoint& position) {
        QMenu menu(this);
        for (int column = 0; column < LibraryModel::ColumnCount; ++column) {
            QAction* action = menu.addAction(m_model->headerData(column, Qt::Horizontal).toString());
            action->setCheckable(true);
            action->setChecked(!m_table->isColumnHidden(column));
            connect(action, &QAction::toggled, this, [this, column](bool shown) {
                m_table->setColumnHidden(column, !shown);
                static_cast<GameTable*>(m_table)->fitLastColumn();
            });
        }
        menu.exec(columns->mapToGlobal(position));
    });
    showEmpty(true);
}

LibraryPage::~LibraryPage()
{
    stopScan();
}

void LibraryPage::setFolders(const QStringList& folders, bool force)
{
    if (!force && folders == m_folders && m_generation > 0)
        return;
    m_folders = folders;
    rescan();
}

void LibraryPage::stopScan()
{
    if (!m_thread)
        return;
    m_cancel = true;
    m_thread->wait();
    m_thread = nullptr; // deletes itself once finished() is delivered
    m_scanning = false;
}

void LibraryPage::rescan()
{
    stopScan();
    const int generation = ++m_generation;
    m_cancel = false;
    m_scanning = true;
    showEmpty(m_model->rowCount() == 0);
    if (!m_loadError.isEmpty()) {
        emit message(m_loadError);
        m_loadError.clear();
    }
    if (m_folders.isEmpty()) {
        scanFinished(LibraryScan(), generation);
        return;
    }
    const QStringList folders = m_folders;
    const QMap<QString, LibraryCachedFile> cache = m_data.files;
    m_thread = QThread::create([this, folders, cache, generation] {
        LibraryScan scan = scanLibrary(folders, cache, &m_cancel);
        QMetaObject::invokeMethod(this, [this, scan = std::move(scan), generation] { scanFinished(scan, generation); }, Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();
}

void LibraryPage::scanFinished(const LibraryScan& scan, int generation)
{
    if (generation != m_generation || scan.cancelled)
        return;
    m_scanning = false;
    m_data.files = scan.files;
    m_model->setEntries(scan.entries, m_data.stats);
    saveData();
    showEmpty(scan.entries.empty());
    if (!scan.missingFolders.isEmpty())
        emit message("Library folder not found: " + QDir::toNativeSeparators(scan.missingFolders.join(", ")));
    else if (!scan.entries.empty())
        emit message(QString("Library: %1 games%2").arg(scan.entries.size()).arg(scan.duplicateFiles > 0 ? QString(" (%1 copies not listed)").arg(scan.duplicateFiles) : QString()));
}

void LibraryPage::recordSession(const QString& gameId, qint64 durationMs)
{
    m_data.addSession(gameId, durationMs, QDateTime::currentMSecsSinceEpoch());
    m_model->setStats(m_data.stats);
    saveData();
}

void LibraryPage::saveData()
{
    QString error;
    if (!m_data.save(AppPaths::libraryPath(), &error))
        emit message(error);
}

void LibraryPage::showEmpty(bool empty)
{
    m_stack->setCurrentIndex(empty ? 0 : 1);
    m_title->setText(empty ? "Welcome to Xfusion" : "Library");
    m_search->setEnabled(!empty);
    m_system->setEnabled(!empty);
    if (m_scanning)
        m_emptyHint->setText("Looking for games...");
    else if (m_folders.isEmpty())
        m_emptyHint->setText("Add a folder with game files (.md, .gen, .smd, .bin, .sms, .gg, .sg, .32x, .cue, .iso, .chd or .zip) to start.");
    else
        m_emptyHint->setText("No games were found in the library folders. Add another folder, or check the list in Options > Files.");
}

void LibraryPage::updateFilter()
{
    m_proxy->setFilter(m_search->text(), m_system->currentData().toInt());
}

void LibraryPage::activate(const QModelIndex& proxyIndex)
{
    const QString path = proxyIndex.isValid() ? m_proxy->data(m_proxy->index(proxyIndex.row(), 0), LibraryModel::PathRole).toString() : QString();
    if (!path.isEmpty())
        emit gameActivated(path);
}

void LibraryPage::showContextMenu(const QPoint& position)
{
    const QModelIndex index = m_table->indexAt(position);
    if (!index.isValid())
        return;
    const QString path = m_proxy->data(m_proxy->index(index.row(), 0), LibraryModel::PathRole).toString();
    QMenu menu(this);
    menu.addAction("Play", this, [this, index] { activate(index); });
    menu.addAction("Show in Folder", this, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
    menu.exec(m_table->viewport()->mapToGlobal(position));
}

void LibraryPage::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, "Choose a game folder");
    if (!folder.isEmpty())
        emit folderChosen(folder);
}
