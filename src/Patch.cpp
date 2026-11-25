#include "Patch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <zlib.h>

#include "ZipRead.h"

namespace {

constexpr qint64 kMaxGameSize = 64 * 1024 * 1024; // larger than any game this frontend plays

quint32 checksum(const QByteArray& bytes, qsizetype length = -1)
{
    const qsizetype size = length < 0 ? bytes.size() : length;
    return quint32(crc32(0, reinterpret_cast<const Bytef*>(bytes.constData()), uInt(size)));
}

// A cursor over a patch that notices when it runs off the end.
class Reader {
public:
    Reader(const QByteArray& bytes, qsizetype position, qsizetype end) : m_bytes(bytes), m_position(position), m_end(end) {}

    bool atEnd() const { return m_position >= m_end; }
    bool failed() const { return m_failed; }
    qsizetype position() const { return m_position; }

    quint8 byte()
    {
        if (m_position >= m_end) {
            m_failed = true;
            return 0;
        }
        return quint8(m_bytes[m_position++]);
    }

    quint64 bigEndian(int count)
    {
        quint64 value = 0;
        for (int i = 0; i < count; ++i)
            value = (value << 8) | byte();
        return value;
    }

    // The variable-length number of BPS and UPS.
    quint64 varint()
    {
        quint64 value = 0, shift = 1;
        while (!m_failed) {
            const quint8 x = byte();
            value += quint64(x & 0x7F) * shift;
            if (x & 0x80)
                break;
            shift <<= 7;
            value += shift;
            if (shift > (quint64(1) << 56)) {
                m_failed = true;
                break;
            }
        }
        return value;
    }

    QByteArray take(qsizetype count)
    {
        if (count < 0 || m_position + count > m_end) {
            m_failed = true;
            return {};
        }
        const QByteArray out = m_bytes.mid(m_position, count);
        m_position += count;
        return out;
    }

private:
    const QByteArray& m_bytes;
    qsizetype m_position;
    qsizetype m_end;
    bool m_failed = false;
};

quint32 littleEndian32(const QByteArray& bytes, qsizetype at)
{
    return quint32(quint8(bytes[at])) | quint32(quint8(bytes[at + 1])) << 8 | quint32(quint8(bytes[at + 2])) << 16 | quint32(quint8(bytes[at + 3])) << 24;
}

PatchResult failure(PatchFormat format, const QString& error)
{
    PatchResult result;
    result.format = format;
    result.error = error;
    return result;
}

PatchResult applyIps(const QByteArray& game, const QByteArray& patch)
{
    const bool wide = patch.startsWith("IPS32");
    const int offsetBytes = wide ? 4 : 3;
    const QByteArray eof = wide ? "EEOF" : "EOF";
    QByteArray out = game;
    Reader reader(patch, 5, patch.size()); // after "PATCH" or "IPS32"
    const auto damaged = [&] { return failure(PatchFormat::Ips, "The patch is damaged (it ends in the middle of a change)"); };

    while (!reader.atEnd()) {
        const qsizetype start = reader.position();
        if (patch.mid(start, eof.size()) == eof) {
            reader.take(eof.size());
            // Some patches end with the length the game must be cut to.
            if (patch.size() - reader.position() == 3) {
                const quint64 length = reader.bigEndian(3);
                if (length < quint64(out.size()))
                    out.resize(qsizetype(length));
            }
            PatchResult result;
            result.ok = true;
            result.data = out;
            result.format = PatchFormat::Ips;
            return result;
        }
        const quint64 offset = reader.bigEndian(offsetBytes);
        const quint64 size = reader.bigEndian(2);
        QByteArray bytes;
        quint64 length = size;
        if (size == 0) {
            length = reader.bigEndian(2);
            bytes = QByteArray(qsizetype(length), char(reader.byte()));
        } else {
            bytes = reader.take(qsizetype(size));
        }
        if (reader.failed())
            return damaged();
        if (offset + length > quint64(kMaxGameSize))
            return failure(PatchFormat::Ips, "The patch asks for a game larger than 64 MB");
        if (out.size() < qsizetype(offset + length))
            out.resize(qsizetype(offset + length), 0);
        out.replace(qsizetype(offset), bytes.size(), bytes);
    }
    return failure(PatchFormat::Ips, "The patch has no end mark; it is damaged or not an IPS file");
}

PatchResult applyBps(const QByteArray& game, const QByteArray& patch)
{
    constexpr PatchFormat format = PatchFormat::Bps;
    if (patch.size() < 4 + 3 + 12)
        return failure(format, "The patch is too short to be a BPS file");
    const qsizetype footer = patch.size() - 12;
    if (checksum(patch, patch.size() - 4) != littleEndian32(patch, patch.size() - 4))
        return failure(format, "The patch is damaged (its checksum does not match)");
    if (checksum(game) != littleEndian32(patch, footer))
        return failure(format, "This patch is for a different version of the game (the game's checksum does not match)");

    Reader reader(patch, 4, footer);
    const quint64 sourceSize = reader.varint();
    const quint64 targetSize = reader.varint();
    const quint64 metadataSize = reader.varint();
    reader.take(qsizetype(metadataSize));
    if (reader.failed() || sourceSize != quint64(game.size()) || targetSize > quint64(kMaxGameSize))
        return failure(format, "The patch is damaged or is for a game of another size");

    QByteArray out(qsizetype(targetSize), 0);
    qsizetype outputOffset = 0, sourceRelative = 0, targetRelative = 0;
    while (!reader.atEnd() && !reader.failed()) {
        const quint64 data = reader.varint();
        const int command = int(data & 3);
        const quint64 length = (data >> 2) + 1;
        if (reader.failed() || outputOffset + qsizetype(length) > out.size())
            return failure(format, "The patch is damaged (a change runs past the end of the game)");
        switch (command) {
        case 0: // the same bytes as the game has at this position
            if (outputOffset + qsizetype(length) > game.size())
                return failure(format, "The patch is damaged (it reads past the end of the game)");
            for (quint64 i = 0; i < length; ++i, ++outputOffset)
                out[outputOffset] = game[outputOffset];
            break;
        case 1: { // new bytes, stored in the patch
            const QByteArray bytes = reader.take(qsizetype(length));
            if (reader.failed())
                return failure(format, "The patch is damaged (it ends in the middle of a change)");
            out.replace(outputOffset, bytes.size(), bytes);
            outputOffset += bytes.size();
            break;
        }
        case 2:
        case 3: {
            const quint64 relative = reader.varint();
            const qint64 delta = (relative & 1 ? -1 : 1) * qint64(relative >> 1);
            qsizetype& cursor = command == 2 ? sourceRelative : targetRelative;
            cursor += qsizetype(delta);
            if (cursor < 0 || (command == 2 && cursor + qsizetype(length) > game.size()) || (command == 3 && cursor >= outputOffset))
                return failure(format, "The patch is damaged (it copies from outside the game)");
            for (quint64 i = 0; i < length; ++i, ++outputOffset, ++cursor)
                out[outputOffset] = command == 2 ? game[cursor] : out[cursor];
            break;
        }
        }
    }
    if (reader.failed() || outputOffset != out.size())
        return failure(format, "The patch is damaged (it does not produce the whole game)");
    if (checksum(out) != littleEndian32(patch, footer + 4))
        return failure(format, "The patched game does not match what the patch promises; the patch or the game is not the right one");
    PatchResult result;
    result.ok = true;
    result.data = out;
    result.format = format;
    return result;
}

PatchResult applyUps(const QByteArray& game, const QByteArray& patch)
{
    constexpr PatchFormat format = PatchFormat::Ups;
    if (patch.size() < 4 + 2 + 12)
        return failure(format, "The patch is too short to be a UPS file");
    const qsizetype footer = patch.size() - 12;
    if (checksum(patch, patch.size() - 4) != littleEndian32(patch, patch.size() - 4))
        return failure(format, "The patch is damaged (its checksum does not match)");

    Reader reader(patch, 4, footer);
    const quint64 inputSize = reader.varint();
    const quint64 outputSize = reader.varint();
    if (reader.failed() || inputSize > quint64(kMaxGameSize) || outputSize > quint64(kMaxGameSize))
        return failure(format, "The patch is damaged or asks for a game larger than 64 MB");
    const quint32 inputChecksum = littleEndian32(patch, footer);
    const quint32 outputChecksum = littleEndian32(patch, footer + 4);

    // A UPS patch works both ways: it takes the original to the patched game and the patched game back.
    const quint32 gameChecksum = checksum(game);
    quint64 resultSize;
    quint32 resultChecksum;
    if (quint64(game.size()) == inputSize && gameChecksum == inputChecksum) {
        resultSize = outputSize;
        resultChecksum = outputChecksum;
    } else if (quint64(game.size()) == outputSize && gameChecksum == outputChecksum) {
        resultSize = inputSize;
        resultChecksum = inputChecksum;
    } else {
        return failure(format, "This patch is for a different version of the game (the game's checksum does not match)");
    }

    QByteArray out = game;
    out.resize(qsizetype(qMax(inputSize, outputSize)), 0);
    quint64 offset = 0;
    while (!reader.atEnd()) {
        offset += reader.varint();
        for (;;) {
            const quint8 x = reader.byte();
            if (reader.failed())
                return failure(format, "The patch is damaged (it ends in the middle of a change)");
            if (offset < quint64(out.size()))
                out[qsizetype(offset)] = char(quint8(out[qsizetype(offset)]) ^ x);
            ++offset;
            if (x == 0)
                break;
        }
    }
    out.resize(qsizetype(resultSize));
    if (checksum(out) != resultChecksum)
        return failure(format, "The patched game does not match what the patch promises; the patch or the game is not the right one");
    PatchResult result;
    result.ok = true;
    result.data = out;
    result.format = format;
    return result;
}

}

bool patchFormatOf(const QByteArray& patch, PatchFormat* format)
{
    PatchFormat found;
    if (patch.startsWith("PATCH") || patch.startsWith("IPS32"))
        found = PatchFormat::Ips;
    else if (patch.startsWith("BPS1"))
        found = PatchFormat::Bps;
    else if (patch.startsWith("UPS1"))
        found = PatchFormat::Ups;
    else
        return false;
    if (format)
        *format = found;
    return true;
}

PatchResult applyPatch(const QByteArray& game, const QByteArray& patch)
{
    PatchFormat format;
    if (!patchFormatOf(patch, &format))
        return failure(PatchFormat::Ips, "The file is not an IPS, BPS or UPS patch");
    switch (format) {
    case PatchFormat::Ips: return applyIps(game, patch);
    case PatchFormat::Bps: return applyBps(game, patch);
    case PatchFormat::Ups: return applyUps(game, patch);
    }
    return failure(format, "Unknown patch format");
}

bool readGameBytes(const QString& path, QByteArray* data, QString* extension, QString* error)
{
    const QFileInfo info(path);
    if (info.suffix().compare("zip", Qt::CaseInsensitive) == 0) {
        static const QStringList gameExtensions = {"md", "gen", "smd", "bin", "32x", "sms", "gg", "sg", "sc"};
        for (const ZipEntry& entry : zipEntries(path)) {
            const QString entryExtension = QFileInfo(entry.name).suffix().toLower();
            if (!gameExtensions.contains(entryExtension))
                continue;
            *data = zipReadHead(path, entry, int(qMin<quint32>(entry.size, quint32(kMaxGameSize))));
            if (data->size() != qsizetype(entry.size)) {
                *error = QString("The game inside %1 could not be unpacked").arg(info.fileName());
                return false;
            }
            *extension = entryExtension;
            return true;
        }
        *error = QString("There is no game inside %1").arg(info.fileName());
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxGameSize) {
        *error = QString("%1 could not be read").arg(info.fileName());
        return false;
    }
    *data = file.readAll();
    *extension = info.suffix().toLower();
    return true;
}

QString findPatch(const QString& gamePath, const QStringList& folders)
{
    const QString name = QFileInfo(gamePath).completeBaseName();
    for (const QString& folder : folders) {
        const QFileInfoList files = QDir(folder).entryInfoList(QDir::Files, QDir::Name);
        for (const char* extension : {"bps", "ups", "ips"}) {
            // Names are compared without regard to case, as the game file would be on Windows.
            for (const QFileInfo& file : files)
                if (file.completeBaseName().compare(name, Qt::CaseInsensitive) == 0 && file.suffix().compare(extension, Qt::CaseInsensitive) == 0)
                    return file.absoluteFilePath();
        }
    }
    return {};
}
