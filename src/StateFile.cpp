#include "StateFile.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace StateFile {

namespace {

constexpr char kMagic[4] = {'X', 'F', 'S', 'T'};
constexpr quint32 kFormatVersion = 2;
constexpr quint32 kMaxThumbnailBytes = 4 * 1024 * 1024;
constexpr quint32 kMaxHeaderBytes = 64 * 1024;
constexpr qint64 kMaxStateBytes = 512LL * 1024 * 1024;

void appendU32(QByteArray& out, quint32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        out.append(static_cast<char>((value >> shift) & 0xFF));
}

quint32 readU32(const QByteArray& in, int at)
{
    quint32 value = 0;
    for (int i = 0; i < 4; ++i)
        value = (value << 8) | static_cast<quint8>(in[at + i]);
    return value;
}

QString sha1Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

QJsonObject toJson(const StateHeader& header, const QByteArray& raw)
{
    return {{"core_id", header.coreId},
            {"core_name", header.coreName},
            {"game_id", header.gameId},
            {"game_name", header.gameName},
            {"system", header.system},
            {"created", header.created.toUTC().toString(Qt::ISODate)},
            {"raw_size", static_cast<double>(raw.size())},
            {"raw_sha1", sha1Hex(raw)}};
}

StateHeader headerFromJson(const QJsonObject& object)
{
    StateHeader header;
    header.coreId = object.value("core_id").toString();
    header.coreName = object.value("core_name").toString();
    header.gameId = object.value("game_id").toString();
    header.gameName = object.value("game_name").toString();
    header.system = object.value("system").toString();
    header.created = QDateTime::fromString(object.value("created").toString(), Qt::ISODate);
    header.rawSize = static_cast<qint64>(object.value("raw_size").toDouble());
    return header;
}

// Reads the fixed part and the header; `payloadOffset` says where the compressed state starts.
std::optional<QJsonObject> readHeaderObject(QFile& file, qint64* payloadOffset, quint32* formatVersion, QString* error)
{
    const QByteArray fixed = file.read(12);
    if (fixed.size() < 12 || fixed.left(4) != QByteArray(kMagic, 4)) {
        *error = QString("%1 is not a Xfusion save state").arg(file.fileName());
        return std::nullopt;
    }
    const quint32 version = readU32(fixed, 4);
    if (version < 1 || version > kFormatVersion) {
        *error = QString("%1 was written by a newer Xfusion (format %2)").arg(file.fileName()).arg(version);
        return std::nullopt;
    }
    const quint32 headerSize = readU32(fixed, 8);
    if (headerSize == 0 || headerSize > kMaxHeaderBytes) {
        *error = QString("%1 is damaged (bad header size)").arg(file.fileName());
        return std::nullopt;
    }
    const QByteArray headerBytes = file.read(headerSize);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(headerBytes, &parseError);
    if (static_cast<quint32>(headerBytes.size()) != headerSize || !document.isObject()) {
        *error = QString("%1 is damaged (unreadable header)").arg(file.fileName());
        return std::nullopt;
    }
    *payloadOffset = 12 + headerSize;
    // Format 2 has the thumbnail here; the caller reads it with readThumbnail().
    *formatVersion = version;
    return document.object();
}

QImage readThumbnail(QFile& file, quint32 version)
{
    if (version < 2)
        return {};
    const QByteArray sizeBytes = file.read(4);
    if (sizeBytes.size() < 4)
        return {};
    const quint32 size = readU32(sizeBytes, 0);
    if (size == 0)
        return {};
    if (size > kMaxThumbnailBytes) {
        file.seek(file.pos() + size);
        return {};
    }
    QImage image;
    image.loadFromData(file.read(size), "PNG");
    return image;
}

}

bool write(const QString& path, const StateHeader& header, const QByteArray& raw, const QImage& thumbnail, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QString("Cannot create the folder for %1").arg(path);
        return false;
    }

    const QByteArray headerBytes = QJsonDocument(toJson(header, raw)).toJson(QJsonDocument::Compact);
    QByteArray out(kMagic, 4);
    appendU32(out, kFormatVersion);
    appendU32(out, static_cast<quint32>(headerBytes.size()));
    out.append(headerBytes);
    QByteArray png;
    if (!thumbnail.isNull()) {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        thumbnail.save(&buffer, "PNG");
    }
    appendU32(out, static_cast<quint32>(png.size()));
    out.append(png);
    out.append(qCompress(raw, 6));

    // Keep the previous state until the new one is safely in place.
    if (QFile::exists(path)) {
        QFile::remove(path + ".bak");
        QFile::copy(path, path + ".bak");
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(out);
    if (!file.commit()) {
        *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

std::optional<StateHeader> readHeader(const QString& path, QImage* thumbnail)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    qint64 offset = 0;
    quint32 version = 0;
    QString error;
    const auto object = readHeaderObject(file, &offset, &version, &error);
    if (!object)
        return std::nullopt;
    if (thumbnail)
        *thumbnail = readThumbnail(file, version);
    return headerFromJson(*object);
}

std::optional<StateData> read(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QString("Cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    if (file.size() > kMaxStateBytes) {
        *error = QString("%1 is too large to be a save state").arg(path);
        return std::nullopt;
    }
    qint64 offset = 0;
    quint32 version = 0;
    const auto object = readHeaderObject(file, &offset, &version, error);
    if (!object)
        return std::nullopt;

    StateData data;
    data.header = headerFromJson(*object);
    data.thumbnail = readThumbnail(file, version);
    data.raw = qUncompress(file.readAll());
    if (data.raw.isEmpty() || data.raw.size() != data.header.rawSize || sha1Hex(data.raw) != object->value("raw_sha1").toString()) {
        *error = QString("%1 is damaged; its state data does not match its checksum").arg(path);
        return std::nullopt;
    }
    return data;
}

}
