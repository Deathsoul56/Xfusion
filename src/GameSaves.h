#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

// Where one game keeps its battery saves and save states, and what the game is called there.
//
// Saves live in a folder per system chosen by the caller, so two systems never meet. Inside a system the files
// are named after the game file, but a name belongs to the game content that first wrote under it (recorded in
// `saves.json` next to the saves): another game with the same file name gets its own suffixed name instead of
// overwriting the first one. The content is identified by a fingerprint (size plus the start and end of the
// file), so moving or renaming a game file keeps its saves.
class GameSaves {
public:
    // Numbered state slots run 0..kMaxSlot; slots 0..9 are the quick slots.
    static constexpr int kMaxSlot = 999;

    GameSaves(const QString& savesDir, const QString& statesDir, const QString& gamePath);

    // Hex SHA-1 of the fingerprint; empty when the game file cannot be read.
    const QString& gameId() const { return m_gameId; }
    // The name used for the files; changes only if `claim()` finds the name was taken meanwhile.
    const QString& baseName() const { return m_baseName; }
    QString saveRamPath() const;
    QString statePath(int slot) const;
    // The game as it was before the last state load, the automatic snapshot taken before a reset or a long
    // stretch without saving, and the state kept when the game is closed.
    QString undoStatePath() const { return stateFile("undo"); }
    QString oopsStatePath() const { return stateFile("oops"); }
    QString resumeStatePath() const { return stateFile("resume"); }
    // Problems found while reading the ownership file (it was unreadable and is being rebuilt); empty when none.
    const QString& warning() const { return m_warning; }

    enum class ReadResult { Missing, Loaded, Failed };
    ReadResult readSaveRam(QByteArray* data, QString* error) const;
    // Records the game as the owner of its name, then writes atomically. Creates the folders on demand.
    bool writeSaveRam(const QByteArray& data, QString* error);
    // Must be called before the first file of this game is written, by any writer (save RAM or state).
    bool claim(QString* error);

    // The file name stem for a game file: its name without extension, safe on every filesystem.
    static QString sanitizedName(const QString& gamePath);
    static QString fingerprint(const QString& gamePath);

private:
    QString manifestPath() const { return m_savesDir + "/saves.json"; }
    QString stateFile(const QString& suffix) const { return QString("%1/%2.%3.state").arg(m_statesDir, m_baseName, suffix); }
    // Base name -> game id. A missing file is an empty manifest; an unreadable one is too, with `warning` set.
    QMap<QString, QString> loadManifest(QString* warning) const;
    QString chooseBaseName(const QMap<QString, QString>& games) const;

    QString m_savesDir;
    QString m_statesDir;
    QString m_gameId;
    QString m_plainName; // sanitizedName() of the game file, before any suffix
    QString m_baseName;
    QString m_warning;
};
