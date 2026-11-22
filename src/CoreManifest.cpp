#include "CoreManifest.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

constexpr int kManifestVersion = 1;

QString manifestPath(const QString& coresDir) { return coresDir + "/manifest.json"; }

}

QString versionFolderName(const QString& version)
{
    QString name;
    for (const QChar c : version.trimmed())
        name += (c.isLetterOrNumber() && c.unicode() < 128) || c == '.' || c == '-' ? c : QChar('_');
    return name.isEmpty() ? QString("unknown") : name;
}

CoreManifest CoreManifest::load(const QString& coresDir, QString* error)
{
    CoreManifest manifest;
    manifest.m_coresDir = coresDir;
    QFile file(manifestPath(coresDir));
    if (!file.open(QIODevice::ReadOnly))
        return manifest;
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QString("The core manifest %1 could not be read: %2").arg(QDir::toNativeSeparators(file.fileName()), parse.errorString());
        return manifest;
    }
    const QJsonObject cores = document.object().value("cores").toObject();
    for (auto it = cores.begin(); it != cores.end(); ++it) {
        const QJsonObject object = it.value().toObject();
        Entry entry;
        for (const QJsonValue value : object.value("versions").toArray()) {
            const QJsonObject v = value.toObject();
            InstalledCore core{v.value("version").toString(), v.value("sha256").toString(), v.value("path").toString(),
                               v.value("source").toString(), v.value("server_date").toString(), v.value("installed_at").toString()};
            if (!core.version.isEmpty() && !core.path.isEmpty())
                entry.versions.append(core);
        }
        entry.active = object.value("active").toString();
        const bool known = std::any_of(entry.versions.begin(), entry.versions.end(), [&](const InstalledCore& c) { return c.version == entry.active; });
        if (!known)
            entry.active.clear();
        manifest.m_cores[it.key()] = entry;
    }
    return manifest;
}

bool CoreManifest::save(QString* error) const
{
    QJsonObject cores;
    for (auto it = m_cores.begin(); it != m_cores.end(); ++it) {
        QJsonArray versions;
        for (const InstalledCore& c : it->versions)
            versions.append(QJsonObject{{"version", c.version}, {"sha256", c.sha256}, {"path", c.path}, {"source", c.source},
                                        {"server_date", c.serverDate}, {"installed_at", c.installedAt}});
        cores[it.key()] = QJsonObject{{"active", it->active}, {"versions", versions}};
    }
    QDir().mkpath(m_coresDir);
    QSaveFile file(manifestPath(m_coresDir));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(QJsonObject{{"version", kManifestVersion}, {"cores", cores}}).toJson()) < 0 || !file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(QDir::toNativeSeparators(file.fileName()), file.errorString());
        return false;
    }
    return true;
}

const InstalledCore* CoreManifest::find(const QString& coreId, const QString& version) const
{
    const auto entry = m_cores.constFind(coreId);
    if (entry == m_cores.constEnd())
        return nullptr;
    for (const InstalledCore& c : entry->versions)
        if (c.version == version)
            return &c;
    return nullptr;
}

const InstalledCore* CoreManifest::active(const QString& coreId) const
{
    return find(coreId, activeVersion(coreId));
}

QString CoreManifest::activePath(const QString& coreId) const
{
    const InstalledCore* core = active(coreId);
    return core ? QDir(m_coresDir).absoluteFilePath(core->path) : QString();
}

void CoreManifest::add(const QString& coreId, const InstalledCore& core, bool makeActive)
{
    Entry& entry = m_cores[coreId];
    for (InstalledCore& existing : entry.versions) {
        if (existing.version == core.version) {
            existing = core;
            if (makeActive)
                entry.active = core.version;
            return;
        }
    }
    entry.versions.append(core);
    if (makeActive)
        entry.active = core.version;
}

bool CoreManifest::setActive(const QString& coreId, const QString& version)
{
    if (!find(coreId, version))
        return false;
    m_cores[coreId].active = version;
    return true;
}

bool CoreManifest::remove(const QString& coreId, const QString& version)
{
    Entry& entry = m_cores[coreId];
    if (entry.active == version)
        return false;
    for (qsizetype i = 0; i < entry.versions.size(); ++i) {
        if (entry.versions[i].version == version) {
            entry.versions.removeAt(i);
            return true;
        }
    }
    return false;
}
