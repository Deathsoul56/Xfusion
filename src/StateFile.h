#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QString>
#include <optional>

// A save state on disk: what the core serialized, plus enough about where it came from to refuse or warn
// before restoring it into a different game, core or core version.
//
// Layout: "XFST", format version (u32 big endian), header length (u32), the header as UTF-8 JSON, then (format 2)
// the length of a PNG thumbnail (u32) and the thumbnail, then the compressed state. The header carries the SHA-1 of
// the uncompressed state so damage is detected on load. Format 1 files, without a thumbnail, still load.
struct StateHeader {
    QString coreId;
    QString coreName; // library name and version, as the core reports them
    QString gameId; // GameSaves::gameId()
    QString gameName;
    QString system;
    QDateTime created;
    qint64 rawSize = 0;
};

struct StateData {
    StateHeader header;
    QByteArray raw;
    QImage thumbnail; // the picture on screen when the state was saved; null for format 1 files
};

namespace StateFile {

// Writes atomically. An existing file is first kept as `<path>.bak`, so a state is never lost to one bad save.
bool write(const QString& path, const StateHeader& header, const QByteArray& raw, const QImage& thumbnail, QString* error);
// Reads and verifies the whole state; on failure returns nothing and says why.
std::optional<StateData> read(const QString& path, QString* error);
// Only the header and the thumbnail, for listing slots without decompressing anything.
std::optional<StateHeader> readHeader(const QString& path, QImage* thumbnail = nullptr);

}
