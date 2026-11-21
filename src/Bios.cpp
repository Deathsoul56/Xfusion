#include "Bios.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <map>

#include "GameHeader.h"

namespace {

constexpr qint64 kBiosSize = 128 * 1024;

QString regionLetter(BiosRegion region)
{
    switch (region) {
    case BiosRegion::Japan: return "J";
    case BiosRegion::Europe: return "E";
    case BiosRegion::USA: return "U";
    }
    return "U";
}

}

std::optional<BiosFile> identifyBios(const QString& path)
{
    QFile file(path);
    if (QFileInfo(path).size() != kBiosSize || !file.open(QIODevice::ReadOnly))
        return std::nullopt;
    file.seek(0x100);
    const QByteArray block = file.read(0x100);
    const GameHeader header = parseMegaDriveHeader(block);
    if (!header.valid || !header.domesticTitle.contains("BOOT ROM", Qt::CaseInsensitive))
        return std::nullopt;

    BiosFile bios;
    bios.path = path;
    // A BIOS is for one region; a header that lists several (or none) is not a Sega CD BIOS.
    switch (header.regions) {
    case GameHeader::Japan: bios.region = BiosRegion::Japan; break;
    case GameHeader::Europe: bios.region = BiosRegion::Europe; break;
    case GameHeader::Americas: bios.region = BiosRegion::USA; break;
    default: return std::nullopt;
    }
    static const QRegularExpression version("(\\d\\.\\d\\d[A-Z]?)$");
    bios.version = version.match(header.domesticTitle).captured(1);
    return bios;
}

QString biosFileName(BiosRegion region)
{
    return QString("bios_CD_%1.bin").arg(regionLetter(region));
}

QString biosRegionName(BiosRegion region)
{
    switch (region) {
    case BiosRegion::Japan: return "Japan";
    case BiosRegion::Europe: return "Europe";
    case BiosRegion::USA: return "USA";
    }
    return {};
}

bool BiosStatus::has(BiosRegion region) const
{
    switch (region) {
    case BiosRegion::Japan: return japan;
    case BiosRegion::Europe: return europe;
    case BiosRegion::USA: return usa;
    }
    return false;
}

BiosStatus checkBios(const QString& systemDir)
{
    const auto present = [&](BiosRegion region) {
        const auto bios = identifyBios(QDir(systemDir).absoluteFilePath(biosFileName(region)));
        return bios && bios->region == region;
    };
    return {present(BiosRegion::Japan), present(BiosRegion::Europe), present(BiosRegion::USA)};
}

QStringList installBios(const QString& sourceFolder, const QString& systemDir, QString* error)
{
    const BiosStatus have = checkBios(systemDir);
    // The newest version found for each region that is missing.
    std::map<BiosRegion, BiosFile> best;
    for (const QFileInfo& info : QDir(sourceFolder).entryInfoList(QDir::Files, QDir::Name)) {
        const auto bios = identifyBios(info.absoluteFilePath());
        if (!bios || have.has(bios->region))
            continue;
        const auto current = best.find(bios->region);
        if (current == best.end() || bios->version > current->second.version)
            best[bios->region] = *bios;
    }

    QStringList installed;
    if (best.empty())
        return installed;
    if (!QDir().mkpath(systemDir)) {
        if (error)
            *error = QString("Cannot create the folder %1").arg(QDir::toNativeSeparators(systemDir));
        return installed;
    }
    for (const auto& [region, bios] : best) {
        const QString target = QDir(systemDir).absoluteFilePath(biosFileName(region));
        if (QFileInfo::exists(target)) { // not a BIOS of that region: kept, not lost
            QFile::remove(target + ".old");
            QFile::rename(target, target + ".old");
        }
        if (QFile::copy(bios.path, target))
            installed << QString("%1 (from %2)").arg(biosFileName(region), QFileInfo(bios.path).fileName());
        else if (error)
            *error = QString("Cannot write %1").arg(QDir::toNativeSeparators(target));
    }
    return installed;
}
