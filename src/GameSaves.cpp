#include "GameSaves.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

namespace {

constexpr qint64 kFingerprintChunk = 1024 * 1024;
constexpr int kManifestVersion = 1;
constexpr int kMaxNameLength = 120;

// Adds the size and the first and last chunk of the file: cheap even for a disc image, and enough to tell
// different games apart.
bool addFileFingerprint(QCryptographicHash& hash, const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    hash.addData(QByteArray::number(file.size()));
    hash.addData(file.read(kFingerprintChunk));
    if (file.size() > kFingerprintChunk) {
        file.seek(std::max(kFingerprintChunk, file.size() - kFingerprintChunk));
        hash.addData(file.read(kFingerprintChunk));
    }
    return true;
}

// File names compare without regard to case on Windows, so "Sonic" and "SONIC" are the same name.
bool nameTaken(const QMap<QString, QString>& games, const QString& name)
{
    for (auto it = games.begin(); it != games.end(); ++it)
        if (it.key().compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

bool isReservedDeviceName(const QString& name)
{
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", QRegularExpression::CaseInsensitiveOption);
    return reserved.match(name).hasMatch();
}

}

QString GameSaves::sanitizedName(const QString& gamePath)
{
    QString name = QFileInfo(gamePath).completeBaseName();
    for (QChar& c : name)
        if (c.unicode() < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(c))
            c = '_';
    name = name.left(kMaxNameLength);
    // Windows drops trailing dots and spaces from file names, which would make two names collide.
    while (name.endsWith('.') || name.endsWith(' '))
        name.chop(1);
    if (name.isEmpty())
        return "game";
    if (isReservedDeviceName(name))
        name.prepend('_');
    return name;
}

// A .cue sheet is a few lines of text that many discs share word for word, so the files it points at count too.
QString GameSaves::fingerprint(const QString& gamePath)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!addFileFingerprint(hash, gamePath))
        return {};
    if (QFileInfo(gamePath).suffix().compare("cue", Qt::CaseInsensitive) == 0) {
        QFile cue(gamePath);
        if (cue.open(QIODevice::ReadOnly)) {
            static const QRegularExpression fileLine("^\\s*FILE\\s+\"([^\"]+)\"",
                                                     QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
            const QString text = QString::fromUtf8(cue.read(1 << 20));
            const QDir dir = QFileInfo(gamePath).absoluteDir();
            for (auto it = fileLine.globalMatch(text); it.hasNext();)
                addFileFingerprint(hash, dir.filePath(it.next().captured(1)));
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

GameSaves::GameSaves(const QString& savesDir, const QString& statesDir, const QString& gamePath)
    : m_savesDir(savesDir)
    , m_statesDir(statesDir)
    , m_gameId(fingerprint(gamePath))
    , m_plainName(sanitizedName(gamePath))
    , m_baseName(m_plainName)
{
    if (!m_gameId.isEmpty())
        m_baseName = chooseBaseName(loadManifest(&m_warning));
}

QString GameSaves::saveRamPath() const
{
    return m_savesDir + "/" + m_baseName + ".srm";
}

QString GameSaves::statePath(int slot) const
{
    return QString("%1/%2.%3.state").arg(m_statesDir, m_baseName, QString::number(slot).rightJustified(3, '0'));
}

QMap<QString, QString> GameSaves::loadManifest(QString* warning) const
{
    QMap<QString, QString> games;
    QFile file(manifestPath());
    if (!file.exists())
        return games;
    if (!file.open(QIODevice::ReadOnly)) {
        *warning = QString("Cannot read %1: %2").arg(manifestPath(), file.errorString());
        return games;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (document.isNull() || !document.isObject() || document.object().value("version").toInt() != kManifestVersion) {
        *warning = QString("%1 is not a valid save index (%2)").arg(manifestPath(), parseError.errorString());
        return games;
    }
    const QJsonObject object = document.object().value("games").toObject();
    for (auto it = object.begin(); it != object.end(); ++it)
        games.insert(it.key(), it.value().toString());
    return games;
}

// The name this game already owns wins, so saves follow the game when its file is renamed or moved. Otherwise the
// plain name if nobody owns it, then names with a growing piece of the game id.
QString GameSaves::chooseBaseName(const QMap<QString, QString>& games) const
{
    for (auto it = games.begin(); it != games.end(); ++it)
        if (it.value() == m_gameId)
            return it.key();

    if (!nameTaken(games, m_plainName))
        return m_plainName;
    for (const int length : {8, 12, 16, 40}) {
        const QString candidate = QString("%1 (%2)").arg(m_plainName, m_gameId.left(length));
        if (!nameTaken(games, candidate))
            return candidate;
    }
    return QString("%1 (%2)").arg(m_plainName, m_gameId);
}

bool GameSaves::claim(QString* error)
{
    if (m_gameId.isEmpty())
        return true; // an unreadable game file cannot be fingerprinted; keep the plain name

    QString warning;
    QMap<QString, QString> games = loadManifest(&warning);
    const QString base = chooseBaseName(games);
    if (games.value(base) == m_gameId) {
        m_baseName = base;
        return true;
    }

    if (!QDir().mkpath(m_savesDir)) {
        *error = QString("Cannot create the save folder %1").arg(m_savesDir);
        return false;
    }
    // Keep what could not be understood instead of silently replacing it.
    if (!warning.isEmpty()) {
        QFile::remove(manifestPath() + ".bad");
        QFile::copy(manifestPath(), manifestPath() + ".bad");
    }

    games.insert(base, m_gameId);
    QJsonObject owners;
    for (auto it = games.begin(); it != games.end(); ++it)
        owners.insert(it.key(), it.value());
    QSaveFile file(manifestPath());
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QString("Cannot write %1: %2").arg(manifestPath(), file.errorString());
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{"version", kManifestVersion}, {"games", owners}}).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        *error = QString("Cannot write %1: %2").arg(manifestPath(), file.errorString());
        return false;
    }
    m_baseName = base;
    return true;
}

GameSaves::ReadResult GameSaves::readSaveRam(QByteArray* data, QString* error) const
{
    QFile file(saveRamPath());
    if (!file.exists())
        return ReadResult::Missing;
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QString("Cannot read %1: %2").arg(file.fileName(), file.errorString());
        return ReadResult::Failed;
    }
    *data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        *error = QString("Cannot read %1: %2").arg(file.fileName(), file.errorString());
        return ReadResult::Failed;
    }
    return ReadResult::Loaded;
}

bool GameSaves::writeSaveRam(const QByteArray& data, QString* error)
{
    if (!claim(error))
        return false;
    QSaveFile file(saveRamPath());
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QString("Cannot write %1: %2").arg(saveRamPath(), file.errorString());
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        *error = QString("Cannot write %1: %2").arg(saveRamPath(), file.errorString());
        return false;
    }
    return true;
}
