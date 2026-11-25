#pragma once

#include <QAbstractTableModel>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QThread>
#include <QWidget>
#include <atomic>
#include <optional>
#include <vector>

#include "CoreProfile.h"
#include "Library.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableView;
class QShowEvent;

// One row per game; the statistics are looked up by game id so that they follow a game around.
class LibraryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    // The columns of the game list of snes9xrd, with the system added: this is a frontend for several.
    enum Column { FileTitle, TitleColumn, SystemColumn, RegionColumn, Size, Company, TimePlayed, LastPlayed, Serial, ColumnCount };
    enum Role { PathRole = Qt::UserRole + 1, SortRole, SystemRole };

    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(m_entries.size()); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : ColumnCount; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setEntries(std::vector<LibraryEntry> entries, const QMap<QString, LibraryStats>& stats);
    void setStats(const QMap<QString, LibraryStats>& stats);
    const LibraryEntry* entryAt(int row) const { return row >= 0 && row < int(m_entries.size()) ? &m_entries[std::size_t(row)] : nullptr; }

private:
    std::vector<LibraryEntry> m_entries;
    QMap<QString, LibraryStats> m_stats;
};

class LibraryProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit LibraryProxy(QObject* parent = nullptr);
    void setFilter(const QString& text, int system); // system -1 shows all

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

private:
    QString m_text;
    int m_system = -1;
};

// The first thing shown while no game is open: the games found in the library folders, with search, a system filter,
// sorting, play time and last played. The folders are scanned on a thread of their own; the games are only read.
class LibraryPage : public QWidget {
    Q_OBJECT
public:
    explicit LibraryPage(QWidget* parent = nullptr);
    ~LibraryPage() override;

    // The system the user chose for a game, if any: it wins over what the files say.
    std::optional<System> systemChoice(const QString& gameId) const;
    QString coreChoice(const QString& gameId) const { return m_data.coreChoices.value(gameId); }
    // The region forced for a game, if any.
    std::optional<GameRegionChoice> regionChoice(const QString& gameId) const
    {
        const auto it = m_data.regionChoices.constFind(gameId);
        return it == m_data.regionChoices.constEnd() ? std::nullopt : std::optional<GameRegionChoice>(static_cast<GameRegionChoice>(it.value()));
    }
    // Scans again when the list changed (or `force`).
    void setFolders(const QStringList& folders, bool force = false);
    // Adds the time of a play session to the game and saves it, whether or not the game is in the library.
    void recordSession(const QString& gameId, qint64 durationMs);
    // Scans the folders again (F5).
    void refresh() { setFolders(m_folders, true); }
    // Gives the keyboard to the list, so that typing searches it.
    void focusList();

signals:
    void gameActivated(const QString& path);
    void folderChosen(const QString& folder);
    void message(const QString& text);

private:
    void rescan();
    void scanFinished(const LibraryScan& scan, int generation);
    void stopScan();
    void showEmpty(bool empty);
    void updateFilter();
    void activate(const QModelIndex& proxyIndex);
    void showContextMenu(const QPoint& position);
    void chooseFolder();
    void saveData();
    void applyChoices(std::vector<LibraryEntry>& entries) const;
    void chooseSystem(const QString& gameId, std::optional<System> system);
    void setCoreChoice(const QString& gameId, const QString& coreId);
    void setRegionChoice(const QString& gameId, std::optional<GameRegionChoice> region);
    void showEvent(QShowEvent* event) override;

    LibraryModel* m_model;
    LibraryProxy* m_proxy;
    QLabel* m_title;
    QLineEdit* m_search;
    QComboBox* m_system;
    QStackedWidget* m_stack;
    QLabel* m_emptyHint;
    QTableView* m_table;

    LibraryData m_data;
    std::vector<LibraryEntry> m_lastEntries; // as the scan found them, before the user's system choices
    QStringList m_folders;
    QString m_loadError; // reported with the first scan, when something can listen
    QPointer<QThread> m_thread; // deletes itself when it finishes, so a plain pointer would dangle
    std::atomic<bool> m_cancel{false};
    int m_generation = 0;
    bool m_scanning = false;
};
