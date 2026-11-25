#include "LibraryPage.h"

#include <QColor>
#include <QActionGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QElapsedTimer>
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
#include <QShowEvent>
#include <QStackedWidget>
#include <QTableView>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

#include "AppPaths.h"
#include "CoreRules.h"

namespace {

QString sizeText(qint64 bytes)
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(double(bytes) / 1048576.0, 0, 'f', 2);
}

// The words snes9xrd uses: "30 second(s)", "11 minute(s)", "2 hour(s) 5 minute(s)", and "Never" for a game not played.
QString playtimeText(qint64 ms)
{
    if (ms <= 0)
        return QString("Never");
    const qint64 seconds = ms / 1000;
    if (seconds < 60)
        return QString("%1 second(s)").arg(seconds);
    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QString("%1 minute(s)").arg(minutes);
    return QString("%1 hour(s) %2 minute(s)").arg(minutes / 60).arg(minutes % 60);
}

QString lastPlayedText(qint64 ms)
{
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString("dd-MM-yyyy") : QString("Never");
}

// What a file is called in the list: its name with the extension, as snes9xrd shows it; a game on several discs is
// shown once, under the name of the whole game.
QString fileTitle(const LibraryEntry& entry)
{
    return entry.otherDiscs.isEmpty() ? QFileInfo(entry.path).fileName() : entry.name;
}

// A table that Enter opens a game from, as a double-click does, and that shares the width of the window among its
// visible columns, as the game list of snes9xrd does.
class GameTable : public QTableView {
public:
    std::function<void(const QModelIndex&)> activate;

    void setDefaultWidths(const QMap<int, int>& widths) { m_defaults = widths; }

    // Always from the default widths, never from the current ones: hiding a column and showing it again would
    // otherwise pile its share on top of what the others already took.
    void redistributeColumnSpace()
    {
        QVector<int> visible;
        int total = 0;
        for (int column = 0; column < LibraryModel::ColumnCount; ++column) {
            if (isColumnHidden(column))
                continue;
            visible.append(column);
            total += m_defaults.value(column, 100);
        }
        if (visible.isEmpty())
            return;
        const int extra = viewport()->width() - total;
        // Room to spare is shared equally. When the window is narrower than the defaults the two columns of names
        // give way first (down to a floor), and if that is not enough the others shrink in proportion to their width.
        constexpr int kNamesFloor = 150;
        const auto isName = [](int column) { return column == LibraryModel::FileTitle || column == LibraryModel::TitleColumn; };
        int othersBase = 0, nameRoom = 0;
        for (const int column : visible) {
            const int base = m_defaults.value(column, 100);
            if (isName(column))
                nameRoom += std::max(base - kNamesFloor, 0);
            else
                othersBase += base;
        }
        const int deficit = -extra;
        const int namesGive = std::min(deficit, nameRoom);
        const double othersScale = othersBase > 0 && deficit > nameRoom ? std::max(0.0, 1.0 - double(deficit - nameRoom) / othersBase) : 1.0;
        for (int i = 0; i < visible.size(); ++i) {
            const int column = visible[i];
            const int base = m_defaults.value(column, 100);
            int width = base;
            if (extra >= 0)
                width += extra / visible.size() + (i == visible.size() - 1 ? extra % visible.size() : 0);
            else if (isName(column))
                width = base - (nameRoom > 0 ? int(qint64(namesGive) * std::max(base - kNamesFloor, 0) / nameRoom) : 0);
            else
                width = int(base * othersScale);
            setColumnWidth(column, std::max(width, std::max(horizontalHeader()->minimumSectionSize(), 60)));
        }
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QTableView::resizeEvent(event);
        redistributeColumnSpace();
    }

    // Typing selects the game whose name starts with what was typed (the letters of the last second), whichever
    // column was clicked last. The same letter again moves on to the next game that starts with it.
    void keyboardSearch(const QString& typed) override
    {
        if (typed.isEmpty() || !model() || model()->rowCount() == 0)
            return;
        if (!m_typedClock.isValid() || m_typedClock.elapsed() > 1000)
            m_typed.clear();
        m_typedClock.restart();
        m_typed += typed;

        const bool sameLetter = m_typed.size() > 1 && m_typed.count(m_typed.at(0)) == m_typed.size();
        const QString needle = sameLetter ? QString(m_typed.at(0)) : m_typed;
        const int rows = model()->rowCount();
        const int current = currentIndex().isValid() ? currentIndex().row() : -1;
        // A longer word may stay on the game it already found; a repeated letter moves on.
        const int first = current < 0 ? 0 : (sameLetter ? current + 1 : current);
        for (int step = 0; step < rows; ++step) {
            const int row = (first + step) % rows;
            const QModelIndex name = model()->index(row, LibraryModel::FileTitle);
            if (model()->data(name).toString().startsWith(needle, Qt::CaseInsensitive)) {
                setCurrentIndex(name);
                scrollTo(name);
                return;
            }
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && activate && currentIndex().isValid()) {
            activate(currentIndex());
            return;
        }
        QTableView::keyPressEvent(event);
    }

private:
    QMap<int, int> m_defaults;
    QString m_typed;
    QElapsedTimer m_typedClock;
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
        case FileTitle: return fileTitle(*entry).toLower();
        case TitleColumn: return entry->title.toLower();
        case SystemColumn: return systemDef(entry->system).folder;
        case RegionColumn: return entry->region;
        case Size: return entry->size;
        case Company: return entry->company.toLower();
        case TimePlayed: return stats.playtimeMs;
        case LastPlayed: return stats.lastPlayedMs;
        default: return entry->serial.toLower();
        }
    }
    if (role == Qt::ToolTipRole) {
        QString text = QDir::toNativeSeparators(entry->path);
        if (!entry->title.isEmpty() && entry->title.compare(entry->name, Qt::CaseInsensitive) != 0)
            text = entry->title + "\n" + text;
        if (!entry->problem.isEmpty())
            text += "\nProblem: " + entry->problem;
        for (int i = 0; i < entry->otherDiscs.size(); ++i)
            text += QString("\nDisc %1: %2").arg(i + 2).arg(QDir::toNativeSeparators(entry->otherDiscs[i]));
        for (const QString& other : entry->duplicates)
            text += "\nAlso found at " + QDir::toNativeSeparators(other);
        return text;
    }
    if (role == Qt::ForegroundRole && !entry->problem.isEmpty() && index.column() == FileTitle)
        return QColor(Qt::darkYellow);
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case FileTitle: return fileTitle(*entry);
    case TitleColumn: return entry->title;
    case SystemColumn: return entry->systemSet ? systemDef(entry->system).folder + " (set)" : systemDef(entry->system).folder;
    case RegionColumn: return entry->region;
    case Size: return sizeText(entry->size);
    case Company: return entry->company;
    case TimePlayed: return playtimeText(stats.playtimeMs);
    case LastPlayed: return lastPlayedText(stats.lastPlayedMs);
    default: return entry->serial;
    }
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* const titles[] = {"File Title", "Title", "System", "Region", "Size", "Company", "Time Played", "Last Played", "Serial"};
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
    const QModelIndex name = sourceModel()->index(row, LibraryModel::FileTitle, parent);
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
    // The widths of snes9xrd, with the system and a wider region (ours are longer); whatever the window has beyond them is shared out.
    const QMap<int, int> widths{{LibraryModel::FileTitle, 500}, {LibraryModel::TitleColumn, 350}, {LibraryModel::SystemColumn, 130}, {LibraryModel::RegionColumn, 120},
                                {LibraryModel::Size, 95}, {LibraryModel::Company, 140}, {LibraryModel::TimePlayed, 110}, {LibraryModel::LastPlayed, 110},
                                {LibraryModel::Serial, 100}};
    static_cast<GameTable*>(m_table)->setDefaultWidths(widths);
    for (auto it = widths.begin(); it != widths.end(); ++it)
        m_table->setColumnWidth(it.key(), it.value());
    m_table->sortByColumn(LibraryModel::FileTitle, Qt::AscendingOrder);

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
                static_cast<GameTable*>(m_table)->redistributeColumnSpace();
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
    m_lastEntries = scan.entries;
    std::vector<LibraryEntry> entries = scan.entries;
    applyChoices(entries);
    m_model->setEntries(std::move(entries), m_data.stats);
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

std::optional<System> LibraryPage::systemChoice(const QString& gameId) const
{
    const auto it = m_data.systemChoices.constFind(gameId);
    return it == m_data.systemChoices.constEnd() ? std::nullopt : std::optional<System>(static_cast<System>(it.value()));
}

void LibraryPage::chooseSystem(const QString& gameId, std::optional<System> system)
{
    if (system)
        m_data.systemChoices[gameId] = static_cast<int>(*system);
    else
        m_data.systemChoices.remove(gameId);
    saveData();
    // The list is rebuilt from the last scan, with the choice applied.
    std::vector<LibraryEntry> entries = m_lastEntries;
    applyChoices(entries);
    m_model->setEntries(std::move(entries), m_data.stats);
}

void LibraryPage::setCoreChoice(const QString& gameId, const QString& coreId)
{
    if (coreId.isEmpty())
        m_data.coreChoices.remove(gameId);
    else
        m_data.coreChoices[gameId] = coreId;
    saveData();
}

void LibraryPage::setRegionChoice(const QString& gameId, std::optional<GameRegionChoice> region)
{
    if (region)
        m_data.regionChoices[gameId] = static_cast<int>(*region);
    else
        m_data.regionChoices.remove(gameId);
    saveData();
}

void LibraryPage::applyChoices(std::vector<LibraryEntry>& entries) const
{
    for (LibraryEntry& entry : entries) {
        const auto choice = systemChoice(entry.id);
        entry.systemSet = choice.has_value();
        if (choice)
            entry.system = *choice;
    }
}

void LibraryPage::saveData()
{
    QString error;
    if (!m_data.save(AppPaths::libraryPath(), &error))
        emit message(error);
}

void LibraryPage::focusList()
{
    if (!m_search->hasFocus() && m_stack->currentWidget() == m_table)
        m_table->setFocus();
}

void LibraryPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    focusList();
}

void LibraryPage::showEmpty(bool empty)
{
    m_stack->setCurrentIndex(empty ? 0 : 1);
    if (!empty)
        focusList();
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
    const QModelIndex source = m_proxy->mapToSource(m_proxy->index(index.row(), 0));
    const LibraryEntry* entry = m_model->entryAt(source.row());
    QMenu menu(this);
    menu.addAction("Play", this, [this, index] { activate(index); });
    if (entry) {
        QMenu* systems = menu.addMenu("System");
        systems->setToolTip("For a game the detection got wrong. The saves of the game go in the folder of the system chosen.");
        const QString id = entry->id;
        auto* group = new QActionGroup(systems);
        QAction* automatic = systems->addAction("Detect automatically", this, [this, id] { chooseSystem(id, std::nullopt); });
        automatic->setCheckable(true);
        automatic->setChecked(!entry->systemSet);
        group->addAction(automatic);
        systems->addSeparator();
        for (const SystemDef& def : allSystems()) {
            QAction* action = systems->addAction(def.folder, this, [this, id, system = def.id] { chooseSystem(id, system); });
            action->setCheckable(true);
            action->setChecked(entry->systemSet && entry->system == def.id);
            group->addAction(action);
        }

        QMenu* cores = menu.addMenu("Core");
        cores->setToolTip("The core that plays this game. By default the one chosen for its system in Options > Emulation > Cores.");
        auto* coreGroup = new QActionGroup(cores);
        const QString chosenCore = coreChoice(id);
        QAction* systemDefault = cores->addAction("Use the system's core", this, [this, id] { setCoreChoice(id, QString()); });
        systemDefault->setCheckable(true);
        systemDefault->setChecked(chosenCore.isEmpty());
        coreGroup->addAction(systemDefault);
        cores->addSeparator();
        for (const CoreInfo& core : allCores()) {
            if (!core.supports(entry->system))
                continue;
            QAction* action = cores->addAction(core.name, this, [this, id, coreId = core.id] { setCoreChoice(id, coreId); });
            action->setCheckable(true);
            action->setChecked(chosenCore == core.id);
            coreGroup->addAction(action);
        }

        QMenu* regions = menu.addMenu("Region");
        regions->setToolTip("The console this game runs on. PAL is 50 Hz and NTSC 60 Hz; the wrong one makes a game run too fast or too slow. By default the one in Options > Emulation > Core Options.");
        auto* regionGroup = new QActionGroup(regions);
        const std::optional<GameRegionChoice> chosenRegion = regionChoice(id);
        QAction* regionDefault = regions->addAction("Use the core setting", this, [this, id] { setRegionChoice(id, std::nullopt); });
        regionDefault->setCheckable(true);
        regionDefault->setChecked(!chosenRegion);
        regionGroup->addAction(regionDefault);
        regions->addSeparator();
        const std::pair<GameRegionChoice, const char*> offered[] = {{GameRegionChoice::Japan, "Japan (NTSC)"}, {GameRegionChoice::USA, "USA (NTSC)"}, {GameRegionChoice::Europe, "Europe (PAL)"}};
        for (const auto& [region, text] : offered) {
            QAction* action = regions->addAction(text, this, [this, id, region = region] { setRegionChoice(id, region); });
            action->setCheckable(true);
            action->setChecked(chosenRegion && *chosenRegion == region);
            regionGroup->addAction(action);
        }
    }
    menu.addAction("Show in Folder", this, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
    menu.exec(m_table->viewport()->mapToGlobal(position));
}

void LibraryPage::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, "Choose a game folder");
    if (!folder.isEmpty())
        emit folderChosen(folder);
}
