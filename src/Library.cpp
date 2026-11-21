#include "Library.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <algorithm>

#include "DiscInfo.h"
#include "GameHeader.h"
#include "GameSaves.h"

namespace {

constexpr int kFileVersion = 1;
// Bumped when the scan learns something new about a file, so that the cache from before is not trusted.
constexpr int kScanVersion = 3;

QStringList namePatterns()
{
    QStringList patterns = {"*.zip"};
    for (const SystemDef& def : allSystems())
        for (const QString& ext : def.extensions)
            patterns << "*." + ext;
    return patterns;
}

// The files a cue sheet refers to, as canonical paths.
QStringList cueTracks(const QString& cuePath)
{
    QFile file(cuePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    static const QRegularExpression fileLine("^\\s*FILE\\s+(?:\"([^\"]+)\"|(\\S+))", QRegularExpression::CaseInsensitiveOption);
    const QDir folder = QFileInfo(cuePath).absoluteDir();
    QStringList tracks;
    while (!file.atEnd()) {
        const QRegularExpressionMatch match = fileLine.match(QString::fromUtf8(file.readLine()));
        if (!match.hasMatch())
            continue;
        const QString name = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        const QString canonical = QFileInfo(folder.absoluteFilePath(name)).canonicalFilePath();
        if (!canonical.isEmpty())
            tracks << canonical;
    }
    return tracks;
}

}

LibraryData LibraryData::load(const QString& path, QString* error)
{
    LibraryData data;
    QFile file(path);
    if (!file.exists())
        return data;
    QJsonParseError parseError;
    const QJsonDocument document = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll(), &parseError) : QJsonDocument();
    if (!document.isObject() || document.object().value("version").toInt() != kFileVersion) {
        data.m_unreadable = true;
        if (error)
            *error = QString("The library file %1 could not be read; a new one will be started.").arg(QDir::toNativeSeparators(path));
        return data;
    }
    const QJsonObject root = document.object();
    const QJsonObject stats = root.value("stats").toObject();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        data.stats[it.key()] = {qint64(entry.value("playtime_ms").toDouble()), qint64(entry.value("last_played_ms").toDouble())};
    }
    for (const QJsonValue& value : root.value("scan_version").toInt() == kScanVersion ? root.value("files").toArray() : QJsonArray()) {
        const QJsonObject entry = value.toObject();
        LibraryCachedFile cached;
        cached.path = entry.value("path").toString();
        cached.size = qint64(entry.value("size").toDouble());
        cached.mtimeMs = qint64(entry.value("mtime_ms").toDouble());
        cached.id = entry.value("id").toString();
        cached.system = entry.value("system").toInt(-1);
        cached.title = entry.value("title").toString();
        cached.region = entry.value("region").toString();
        cached.problem = entry.value("problem").toString();
        cached.contentSize = qint64(entry.value("content_size").toDouble());
        if (!cached.path.isEmpty())
            data.files[cached.path] = cached;
    }
    return data;
}

bool LibraryData::save(const QString& path, QString* error)
{
    if (m_unreadable) {
        QFile::remove(path + ".bad");
        QFile::copy(path, path + ".bad");
        m_unreadable = false;
    }
    QJsonObject statsJson;
    for (auto it = stats.begin(); it != stats.end(); ++it)
        statsJson[it.key()] = QJsonObject{{"playtime_ms", double(it->playtimeMs)}, {"last_played_ms", double(it->lastPlayedMs)}};
    QJsonArray filesJson;
    for (const LibraryCachedFile& cached : files)
        filesJson.append(QJsonObject{{"path", cached.path}, {"size", double(cached.size)}, {"mtime_ms", double(cached.mtimeMs)},
                                 {"id", cached.id}, {"system", cached.system}, {"title", cached.title}, {"region", cached.region},
                                 {"problem", cached.problem}, {"content_size", double(cached.contentSize)}});

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{"version", kFileVersion}, {"scan_version", kScanVersion}, {"stats", statsJson}, {"files", filesJson}}).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

void LibraryData::addSession(const QString& id, qint64 durationMs, qint64 playedAtMs)
{
    if (id.isEmpty() || durationMs < 0)
        return;
    LibraryStats& entry = stats[id];
    entry.playtimeMs += durationMs;
    entry.lastPlayedMs = std::max(entry.lastPlayedMs, playedAtMs);
}

LibraryScan scanLibrary(const QStringList& folders, const QMap<QString, LibraryCachedFile>& cache, const std::atomic<bool>* cancel,
                        const std::function<void(int)>& progress)
{
    LibraryScan scan;
    const auto stop = [&] { return cancel && cancel->load(std::memory_order_relaxed); };

    // Every candidate file once, by canonical path.
    QSet<QString> seenFolders;
    QMap<QString, QFileInfo> candidates;
    const QStringList patterns = namePatterns();
    for (const QString& folder : folders) {
        const QFileInfo info(folder);
        const QString canonicalFolder = info.canonicalFilePath();
        if (!info.isDir() || canonicalFolder.isEmpty()) {
            scan.missingFolders << folder;
            continue;
        }
        if (seenFolders.contains(canonicalFolder))
            continue;
        seenFolders.insert(canonicalFolder);
        QDirIterator it(canonicalFolder, patterns, QDir::Files | QDir::Readable, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext()) {
            if (stop()) {
                scan.cancelled = true;
                return scan;
            }
            it.next();
            const QString canonical = it.fileInfo().canonicalFilePath();
            if (!canonical.isEmpty() && !candidates.contains(canonical))
                candidates.insert(canonical, it.fileInfo());
        }
    }

    QSet<QString> tracks;
    for (auto it = candidates.begin(); it != candidates.end(); ++it)
        if (it.value().suffix().compare("cue", Qt::CaseInsensitive) == 0)
            for (const QString& track : cueTracks(it.key()))
                tracks.insert(track);

    std::vector<LibraryEntry> found;
    int examined = 0;
    for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        if (stop()) {
            scan.cancelled = true;
            return scan;
        }
        if (progress && ++examined % 16 == 0)
            progress(examined);
        if (tracks.contains(it.key()))
            continue;

        const QFileInfo& info = it.value();
        LibraryCachedFile cached = cache.value(it.key());
        const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
        if (cached.path != it.key() || cached.size != info.size() || cached.mtimeMs != mtimeMs) {
            cached = {it.key(), info.size(), mtimeMs, {}, -1};
            if (const auto system = systemForFile(it.key())) {
                cached.id = GameSaves::fingerprint(it.key());
                if (!cached.id.isEmpty()) {
                    cached.system = static_cast<int>(*system);
                    GameHeader header;
                    if (*system == System::SegaCD) {
                        const DiscInfo disc = readDisc(it.key());
                        header = disc.header;
                        cached.problem = disc.problems.join("; ");
                        cached.contentSize = disc.totalSize;
                    } else {
                        header = readCartridgeHeader(it.key());
                    }
                    cached.title = header.title();
                    cached.region = header.regionText();
                }
            }
        }
        scan.files.insert(it.key(), cached);
        if (cached.system < 0)
            continue;
        found.push_back({it.key(), cached.id, info.completeBaseName(), cached.title, cached.region, cached.problem,
                         static_cast<System>(cached.system), cached.contentSize > 0 ? cached.contentSize : cached.size, cached.mtimeMs, {}});
    }
    if (progress)
        progress(examined);

    std::sort(found.begin(), found.end(), [](const LibraryEntry& a, const LibraryEntry& b) { return a.path.compare(b.path, Qt::CaseInsensitive) < 0; });
    QMap<QString, std::size_t> firstById;
    for (const LibraryEntry& entry : found) {
        const auto same = firstById.constFind(entry.id);
        if (same == firstById.constEnd()) {
            firstById.insert(entry.id, scan.entries.size());
            scan.entries.push_back(entry);
        } else {
            scan.entries[*same].duplicates << entry.path;
            ++scan.duplicateFiles;
        }
    }
    return scan;
}
