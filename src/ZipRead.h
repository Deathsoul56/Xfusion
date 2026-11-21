#pragma once

#include <QByteArray>
#include <QString>
#include <vector>

// A minimal reader for the games inside a .zip: enough to list the files and to read the start of one, which is all
// the frontend needs to name a game. (Playing a zip is the core's business.)
struct ZipEntry {
    QString name;
    int method = 0; // 0 stored, 8 deflated
    quint32 compressedSize = 0;
    quint32 size = 0;
    quint32 localHeaderOffset = 0;
};

std::vector<ZipEntry> zipEntries(const QString& path);

// The first `maxBytes` bytes of the entry (fewer if it is shorter); empty when it cannot be read or is compressed some
// other way than deflate.
QByteArray zipReadHead(const QString& path, const ZipEntry& entry, int maxBytes);
