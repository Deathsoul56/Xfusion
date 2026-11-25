#include "AchievementsDisc.h"

#include <QString>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>

#include "ChdDisc.h"
#include "rc_hash.h"

namespace {

rc_hash_cdreader_t g_default; // the reader rcheevos brings
std::mutex g_mutex;
std::set<void*> g_ours; // the handles that come from here: a handle of the default reader is not told apart by its contents

struct ChdTrack {
    std::unique_ptr<ChdDisc> disc;
    const ChdDisc::Track* track = nullptr;
};

bool isChd(const char* path)
{
    return QString::fromUtf8(path).endsWith(".chd", Qt::CaseInsensitive);
}

bool ours(void* handle)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ours.count(handle) != 0;
}

const ChdDisc::Track* pickTrack(const ChdDisc& disc, uint32_t wanted)
{
    if (wanted == RC_HASH_CDTRACK_FIRST_DATA)
        return disc.firstDataTrack();
    if (wanted == RC_HASH_CDTRACK_LAST)
        return &disc.tracks().last();
    if (wanted == RC_HASH_CDTRACK_LARGEST || wanted == RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION || wanted == 0) {
        const ChdDisc::Track* largest = &disc.tracks().first();
        for (const ChdDisc::Track& track : disc.tracks())
            if (track.frames > largest->frames)
                largest = &track;
        return largest;
    }
    return disc.track(int(wanted));
}

void* openTrack(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
    if (!isChd(path)) {
        if (g_default.open_track_iterator)
            return g_default.open_track_iterator(path, track, iterator);
        return g_default.open_track ? g_default.open_track(path, track) : nullptr;
    }
    QString error;
    std::unique_ptr<ChdDisc> disc = ChdDisc::open(QString::fromUtf8(path), &error);
    if (!disc)
        return nullptr;
    const ChdDisc::Track* chosen = pickTrack(*disc, track);
    if (!chosen)
        return nullptr;
    auto* handle = new ChdTrack{std::move(disc), chosen};
    std::lock_guard<std::mutex> lock(g_mutex);
    g_ours.insert(handle);
    return handle;
}

size_t readSector(void* handle, uint32_t sector, void* buffer, size_t requestedBytes)
{
    if (!ours(handle))
        return g_default.read_sector ? g_default.read_sector(handle, sector, buffer, requestedBytes) : 0;
    auto* chd = static_cast<ChdTrack*>(handle);
    if (sector < chd->track->startLba)
        return 0;
    const QByteArray data = chd->disc->readTrack(*chd->track, sector - chd->track->startLba, int(requestedBytes));
    std::memcpy(buffer, data.constData(), size_t(data.size()));
    return size_t(data.size());
}

void closeTrack(void* handle)
{
    if (!ours(handle)) {
        if (g_default.close_track)
            g_default.close_track(handle);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_ours.erase(handle);
    }
    delete static_cast<ChdTrack*>(handle);
}

uint32_t firstTrackSector(void* handle)
{
    if (!ours(handle))
        return g_default.first_track_sector ? g_default.first_track_sector(handle) : 0;
    const ChdDisc::Track* track = static_cast<ChdTrack*>(handle)->track;
    return track->startLba + track->pregap;
}

}

void installDiscReader()
{
    rc_hash_get_default_cdreader(&g_default);
    rc_hash_cdreader_t reader{};
    reader.open_track_iterator = openTrack;
    reader.read_sector = readSector;
    reader.close_track = closeTrack;
    reader.first_track_sector = firstTrackSector;
    rc_hash_init_custom_cdreader(&reader);
}
