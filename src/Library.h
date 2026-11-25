#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <vector>

#include "SystemInfo.h"

// A game found in the library folders. The ROM files are only read, never changed or moved.
struct LibraryEntry {
    QString path;
    QString id; // GameSaves::fingerprint(): the same game keeps it when the file is moved or renamed
    QString name; // the file name without extension
    QString title; // the name inside the game (its header), when it has one
    QString region; // "Japan", "USA", "Europe", "World" ...; empty when the game does not say
    QString problem; // something wrong with the files of a disc (a track missing), empty when all is well
    System system = System::Genesis;
    qint64 size = 0; // of the whole game: for a disc, all its track files
    qint64 mtimeMs = 0;
    QStringList duplicates; // other files with the same content; they are not listed separately
    bool systemSet = false; // `system` is what the user chose, not what was detected
    // The other discs of a game on several discs (Sega CD), in disc order: the entry is the first disc found and the
    // rest are not listed separately. The Disc menu changes between them while the game runs.
    QStringList otherDiscs;
    QString company; // from the copyright of the header, when it has one
    QString serial; // the product code of the header
};

// What is remembered about a file between scans, so that a rescan does not open every game again. `system` is -1 for
// a file that turned out not to be a game (for instance a zip with something else inside).
struct LibraryCachedFile {
    QString path;
    qint64 size = 0;
    qint64 mtimeMs = 0;
    QString id;
    int system = -1;
    QString title;
    QString region;
    QString company;
    QString serial;
    QString problem;
    qint64 contentSize = 0; // what the game takes on disk when that is more than this file (a disc with its tracks)
};

struct LibraryStats {
    qint64 playtimeMs = 0;
    qint64 lastPlayedMs = 0; // 0 = never
};

// The part of the library that has to survive restarts: the play statistics, kept by game id so that they outlive a
// game being moved, removed from a folder for a while or found in two places, and the scan cache.
class LibraryData {
public:
    QMap<QString, LibraryStats> stats;
    // The system the user chose for a game, by game id, when the detection got it wrong (or the file says nothing).
    QMap<QString, int> systemChoices;
    // The core the user chose for one game, by game id (a CoreId); it wins over the choice for the whole system.
    QMap<QString, QString> coreChoices;
    // The region the user forced for one game, by game id: 0 Japan, 1 USA, 2 Europe (a GameRegionChoice).
    QMap<QString, int> regionChoices;
    QMap<QString, LibraryCachedFile> files; // by canonical path

    // A missing file is an empty library. An unreadable one is too, with `error` set; it is then kept as
    // `<file>.bad` the first time something is saved over it.
    static LibraryData load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr);

    void addSession(const QString& id, qint64 durationMs, qint64 playedAtMs);

private:
    bool m_unreadable = false;
};

struct LibraryScan {
    std::vector<LibraryEntry> entries; // sorted by path
    QMap<QString, LibraryCachedFile> files;
    QStringList missingFolders; // configured folders that do not exist (or are not folders)
    int duplicateFiles = 0;
    bool cancelled = false;
};

// Looks through the folders and their subfolders for games. A file reached twice (nested or repeated folders,
// links) counts once, and files with the same content are listed once with the others in `duplicates`. The tracks
// that a .cue sheet names are not games. `progress` receives how many files were examined so far.
LibraryScan scanLibrary(const QStringList& folders, const QMap<QString, LibraryCachedFile>& cache,
                        const std::atomic<bool>* cancel = nullptr, const std::function<void(int)>& progress = {});
