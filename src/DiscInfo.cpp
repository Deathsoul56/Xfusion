#include "DiscInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

constexpr char kSignature[] = "SEGADISCSYSTEM";
constexpr int kSignatureLength = sizeof(kSignature) - 1;
const QByteArray kSync("\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00", 12);

// Reads the first sector of a data track and fills what it says. False when the file is not a Sega CD data track.
bool readDataTrack(const QString& path, DiscInfo* info)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray start = file.read(0x30 + 0x200);
    int user = -1;
    if (start.startsWith(kSignature)) {
        user = 0;
        info->format = "ISO, 2048-byte sectors";
    } else if (start.startsWith(kSync) && start.mid(16, kSignatureLength) == kSignature) {
        user = 16; // mode 1: 12 bytes of sync and 4 of header come first
        info->format = "BIN/CUE, 2352-byte sectors";
    } else if (start.startsWith(kSync) && start.mid(24, kSignatureLength) == kSignature) {
        user = 24; // mode 2 form 1 has 8 more bytes of sub-header
        info->format = "BIN/CUE, 2352-byte sectors (mode 2)";
    }
    if (user < 0)
        return false;
    info->dataFile = path;
    file.seek(user + 0x10);
    info->volume = QString::fromLatin1(file.read(11)).simplified();
    file.seek(user + 0x100);
    info->header = parseMegaDriveHeader(file.read(0x100));
    info->isSegaCd = true;
    return true;
}

// The files that sit beside an .iso as its audio tracks: "Name 02.mp3", "Name 03.mp3", ...
QStringList siblingAudio(const QString& isoPath)
{
    const QFileInfo iso(isoPath);
    const QRegularExpression pattern("^" + QRegularExpression::escape(iso.completeBaseName()) + "[ _.-]*(\\d{1,3})\\.(mp3|wav|ogg|flac)$",
                                     QRegularExpression::CaseInsensitiveOption);
    QStringList files;
    for (const QFileInfo& candidate : iso.absoluteDir().entryInfoList(QDir::Files, QDir::Name)) {
        const QRegularExpressionMatch match = pattern.match(candidate.fileName());
        if (match.hasMatch() && match.captured(1).toInt() >= 2)
            files << candidate.absoluteFilePath();
    }
    return files;
}

DiscInfo readCue(const QString& cuePath)
{
    DiscInfo info;
    QFile file(cuePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        info.problems << "The cue sheet cannot be read";
        return info;
    }
    static const QRegularExpression fileLine("^\\s*FILE\\s+(?:\"([^\"]+)\"|(\\S+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression trackLine("^\\s*TRACK\\s+\\d+\\s+(\\S+)", QRegularExpression::CaseInsensitiveOption);
    const QDir folder = QFileInfo(cuePath).absoluteDir();

    QString currentFile;
    QString dataFile;
    QStringList seenFiles;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine());
        if (const auto match = fileLine.match(line); match.hasMatch()) {
            currentFile = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
            if (!seenFiles.contains(currentFile)) {
                seenFiles << currentFile;
                const QFileInfo track(folder.absoluteFilePath(currentFile));
                if (!track.exists())
                    info.problems << "Missing track file: " + currentFile;
                info.totalSize += track.size();
            }
        } else if (const auto track = trackLine.match(line); track.hasMatch()) {
            if (track.captured(1).compare("AUDIO", Qt::CaseInsensitive) == 0) {
                ++info.audioTracks;
                if (!info.audioFiles.contains(currentFile))
                    info.audioFiles << currentFile;
            } else {
                ++info.dataTracks;
                if (dataFile.isEmpty())
                    dataFile = currentFile;
            }
        }
    }
    if (dataFile.isEmpty()) {
        info.problems << "The cue sheet has no data track";
        return info;
    }
    if (!readDataTrack(folder.absoluteFilePath(dataFile), &info) && QFileInfo::exists(folder.absoluteFilePath(dataFile)))
        info.problems << "The data track does not look like a Sega CD disc";
    return info;
}

}

DiscInfo readDisc(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "cue")
        return readCue(path);

    DiscInfo info;
    if (ext == "chd") {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) && file.read(8) == "MComprHD") {
            info.isSegaCd = true;
            info.format = "CHD (not inspected)";
            info.dataFile = path;
        }
        return info;
    }
    if (!readDataTrack(path, &info))
        return info;
    info.dataTracks = 1;
    info.totalSize = QFileInfo(path).size();
    if (ext == "iso") {
        info.audioFiles = siblingAudio(path);
        info.audioTracks = info.audioFiles.size();
        for (const QString& audio : info.audioFiles)
            info.totalSize += QFileInfo(audio).size();
    }
    return info;
}
