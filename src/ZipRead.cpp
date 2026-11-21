#include "ZipRead.h"

#include <QFile>
#include <zlib.h>

namespace {

quint16 u16(const QByteArray& b, int at) { return quint16(quint8(b[at]) | quint8(b[at + 1]) << 8); }
quint32 u32(const QByteArray& b, int at) { return quint32(u16(b, at)) | quint32(u16(b, at + 2)) << 16; }

}

std::vector<ZipEntry> zipEntries(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    constexpr qint64 eocdSize = 22;
    const qint64 tailSize = qMin<qint64>(file.size(), eocdSize + 0xFFFF);
    file.seek(file.size() - tailSize);
    const QByteArray tail = file.read(tailSize);
    const int eocd = tail.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0 || tail.size() - eocd < eocdSize)
        return {};

    const int count = u16(tail, eocd + 10);
    file.seek(u32(tail, eocd + 16));
    const QByteArray dir = file.read(u32(tail, eocd + 12));

    std::vector<ZipEntry> entries;
    int pos = 0;
    for (int i = 0; i < count && pos + 46 <= dir.size(); ++i) {
        const int nameLen = u16(dir, pos + 28);
        const int extraLen = u16(dir, pos + 30);
        const int commentLen = u16(dir, pos + 32);
        ZipEntry entry;
        entry.method = u16(dir, pos + 10);
        entry.compressedSize = u32(dir, pos + 20);
        entry.size = u32(dir, pos + 24);
        entry.localHeaderOffset = u32(dir, pos + 42);
        entry.name = QString::fromUtf8(dir.mid(pos + 46, nameLen));
        entries.push_back(entry);
        pos += 46 + nameLen + extraLen + commentLen;
    }
    return entries;
}

QByteArray zipReadHead(const QString& path, const ZipEntry& entry, int maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(entry.localHeaderOffset))
        return {};
    const QByteArray local = file.read(30);
    if (local.size() < 30 || !local.startsWith(QByteArray("PK\x03\x04", 4)))
        return {};
    // The local header repeats the name and extra field, whose lengths may differ from the central directory's.
    if (!file.seek(qint64(entry.localHeaderOffset) + 30 + u16(local, 26) + u16(local, 28)))
        return {};

    const int wanted = int(qMin<quint32>(entry.size, quint32(maxBytes)));
    if (entry.method == 0)
        return file.read(wanted);
    if (entry.method != 8)
        return {};

    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return {};
    QByteArray out(wanted, 0);
    QByteArray in;
    int status = Z_OK;
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = uInt(out.size());
    qint64 remaining = entry.compressedSize;
    while (status == Z_OK && stream.avail_out > 0 && remaining > 0) {
        in = file.read(qMin<qint64>(remaining, 16384));
        if (in.isEmpty())
            break;
        remaining -= in.size();
        stream.next_in = reinterpret_cast<Bytef*>(in.data());
        stream.avail_in = uInt(in.size());
        status = inflate(&stream, Z_NO_FLUSH);
    }
    const int produced = int(out.size() - int(stream.avail_out));
    inflateEnd(&stream);
    if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
        return {};
    out.truncate(produced);
    return out;
}
