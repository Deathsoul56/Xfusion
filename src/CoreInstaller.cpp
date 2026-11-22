#include "CoreInstaller.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>

#include "AppPaths.h"
#include "CoreManifest.h"
#include "CoreProbe.h"
#include "CoreProfile.h"
#include "CoreRules.h"
#include "ZipRead.h"

namespace {

constexpr qint64 kMaxDownload = 64 * 1024 * 1024;
constexpr int kTimeoutMs = 60'000;

QNetworkRequest makeRequest(const QString& url)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Xfusion");
    return request;
}

QString isoDate(const QDateTime& date) { return date.isValid() ? date.toUTC().toString(Qt::ISODate) : QString(); }

}

CoreInstaller::CoreInstaller(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString CoreInstaller::downloadUrl(const QString& coreId)
{
    const CoreInfo* info = coreInfo(coreId);
    if (!info)
        return {};
#if defined(Q_OS_WIN)
    const QString platform = "windows/x86_64";
#elif defined(Q_OS_LINUX)
    const QString platform = "linux/x86_64";
#else
    return {};
#endif
    return QString("https://buildbot.libretro.com/nightly/%1/latest/%2.zip").arg(platform, coreProfileById(coreId).libraryFileName());
}

void CoreInstaller::checkForUpdate(const QString& coreId)
{
    const QString url = downloadUrl(coreId);
    if (busy() || url.isEmpty()) {
        emit checked(coreId, {}, busy() ? "Another download is running" : "There is no build of this core for this system");
        return;
    }
    emit progress("Asking the server...");
    m_reply = m_network->head(makeRequest(url));
    connect(m_reply, &QNetworkReply::finished, this, [this, coreId] {
        QNetworkReply* reply = std::exchange(m_reply, nullptr);
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            emit checked(coreId, {}, reply->errorString());
        else
            emit checked(coreId, isoDate(reply->header(QNetworkRequest::LastModifiedHeader).toDateTime()), {});
    });
}

void CoreInstaller::install(const QString& coreId)
{
    const QString url = downloadUrl(coreId);
    if (busy() || url.isEmpty()) {
        emit finished(coreId, {}, false, busy() ? "Another download is running" : "There is no build of this core for this system");
        return;
    }
    emit progress("Downloading...");
    m_reply = m_network->get(makeRequest(url));
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > kMaxDownload || received > kMaxDownload) {
            m_reply->abort();
            return;
        }
        emit progress(total > 0 ? QString("Downloading... %1 of %2 KB").arg(received / 1024).arg(total / 1024) : QString("Downloading... %1 KB").arg(received / 1024));
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, coreId, url] {
        QNetworkReply* reply = std::exchange(m_reply, nullptr);
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit finished(coreId, {}, false, reply->errorString());
            return;
        }
        finishInstall(coreId, reply->readAll(), isoDate(reply->header(QNetworkRequest::LastModifiedHeader).toDateTime()), url);
    });
}

// The zip holds the core's library; it is unpacked into a staging file, asked what it is, and only then moved to the
// folder of its version, so a bad download never touches the cores that work.
void CoreInstaller::finishInstall(const QString& coreId, const QByteArray& zip, const QString& serverDate, const QString& source)
{
    const auto fail = [&](const QString& why) { emit finished(coreId, {}, false, why); };
    emit progress("Installing...");

    QTemporaryFile zipFile;
    if (!zipFile.open() || zipFile.write(zip) != zip.size() || !zipFile.flush())
        return fail("A temporary file could not be written");
    const QString libraryName = coreProfileById(coreId).libraryFileName();
    const ZipEntry* wanted = nullptr;
    const std::vector<ZipEntry> entries = zipEntries(zipFile.fileName());
    for (const ZipEntry& entry : entries)
        if (QFileInfo(entry.name).fileName().compare(libraryName, Qt::CaseInsensitive) == 0)
            wanted = &entry;
    if (!wanted)
        return fail(QString("The download does not contain %1").arg(libraryName));
    if (wanted->size > kMaxDownload)
        return fail("The core in the download is too big");
    const QByteArray library = zipReadHead(zipFile.fileName(), *wanted, int(wanted->size));
    if (library.size() != qsizetype(wanted->size) || library.isEmpty())
        return fail("The core could not be unpacked from the download");

    const QString coresDir = AppPaths::coresDir();
    const QString staging = QString("%1/%2/.download").arg(coresDir, coreId);
    QDir(staging).removeRecursively();
    QDir().mkpath(staging);
    const QString stagedFile = staging + "/" + libraryName;
    QFile out(stagedFile);
    if (!out.open(QIODevice::WriteOnly) || out.write(library) != library.size())
        return fail(QString("Cannot write %1").arg(QDir::toNativeSeparators(stagedFile)));
    out.close();

    QString name, version, error;
    if (!probeCore(stagedFile, &name, &version, &error)) {
        QDir(staging).removeRecursively();
        return fail(error);
    }
    const QString sha = QString::fromLatin1(QCryptographicHash::hash(library, QCryptographicHash::Sha256).toHex());

    QString manifestError;
    CoreManifest manifest = CoreManifest::load(coresDir, &manifestError);
    if (!manifestError.isEmpty()) {
        QDir(staging).removeRecursively();
        return fail(manifestError);
    }
    for (const InstalledCore& installed : manifest.versions(coreId)) {
        if (installed.sha256 == sha) {
            QDir(staging).removeRecursively();
            manifest.setActive(coreId, installed.version);
            if (!manifest.save(&error))
                return fail(error);
            emit finished(coreId, installed.version, true, {});
            return;
        }
    }

    // Two different builds that call themselves the same version get different folders.
    QString folder = versionFolderName(version);
    if (QDir(QString("%1/%2/%3").arg(coresDir, coreId, folder)).exists())
        folder += "-" + sha.left(8);
    const QString finalDir = QString("%1/%2/%3").arg(coresDir, coreId, folder);
    QDir().mkpath(finalDir);
    const QString finalFile = finalDir + "/" + libraryName;
    QFile::remove(finalFile);
    if (!QFile::rename(stagedFile, finalFile)) {
        QDir(staging).removeRecursively();
        return fail(QString("Cannot move the core to %1").arg(QDir::toNativeSeparators(finalDir)));
    }
    QDir(staging).removeRecursively();

    InstalledCore core;
    core.version = version;
    core.sha256 = sha;
    core.path = QString("%1/%2/%3").arg(coreId, folder, libraryName);
    core.source = source;
    core.serverDate = serverDate;
    core.installedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (manifest.find(coreId, version))
        core.version = version + " (" + sha.left(8) + ")"; // the same name for another build: the manifest tells them apart
    manifest.add(coreId, core, true);
    if (!manifest.save(&error))
        return fail(error);
    emit finished(coreId, core.version, false, {});
}
