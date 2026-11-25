#include "ChdDisc.h"

#include <QFile>
#include <cstdio>

extern "C" {
#include "libchdr/cdrom.h"
#include "libchdr/chd.h"
}

namespace {

FILE* openFile(const QString& path)
{
#ifdef _WIN32
    return _wfopen(reinterpret_cast<const wchar_t*>(path.utf16()), L"rb"); // names with accents and the like
#else
    return std::fopen(QFile::encodeName(path).constData(), "rb");
#endif
}

// What the metadata of a track says: "TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:12345 PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0".
bool parseTrack(const char* text, ChdDisc::Track* track)
{
    int number = 0, frames = 0, pregap = 0;
    char type[32] = {}, subtype[32] = {}, pgtype[32] = {}, pgsub[32] = {};
    int postgap = 0;
    const int found = std::sscanf(text, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d", &number, type, subtype, &frames,
                                  &pregap, pgtype, pgsub, &postgap);
    if (found < 4) // the older tag has no pregap
        return false;
    track->number = number;
    track->type = QString::fromLatin1(type);
    track->audio = track->type == "AUDIO";
    track->frames = quint32(frames);
    // A pregap whose type starts with V is not in the image; any other is part of the frames.
    track->pregap = found >= 6 && pgtype[0] != 'V' ? quint32(pregap) : 0;
    return true;
}

// Where the 2048 bytes of user data start inside a frame of 2352 bytes (or 2048 when the track holds no more than that).
int userOffset(const QString& type)
{
    if (type == "MODE1_RAW")
        return 16;
    if (type == "MODE2_RAW" || type == "MODE2_FORM1_RAW")
        return 24;
    if (type == "MODE2_FORM_MIX")
        return 8;
    return 0; // MODE1 and MODE2_FORM1 hold the data alone
}

}

std::unique_ptr<ChdDisc> ChdDisc::open(const QString& path, QString* error)
{
    std::unique_ptr<ChdDisc> disc(new ChdDisc);
    FILE* file = openFile(path);
    if (!file) {
        *error = "The file cannot be opened";
        return nullptr;
    }
    disc->m_file = file;
    chd_file* chd = nullptr;
    const chd_error status = chd_open_file(file, CHD_OPEN_READ, nullptr, &chd);
    if (status != CHDERR_NONE) {
        *error = QString::fromLatin1(chd_error_string(status));
        return nullptr; // the destructor closes the FILE
    }
    disc->m_chd = chd;
    const chd_header* header = chd_get_header(chd);
    if (!header || header->hunkbytes == 0 || header->hunkbytes % CD_FRAME_SIZE != 0) {
        *error = "This CHD is not the image of a CD";
        return nullptr;
    }
    disc->m_framesPerHunk = header->hunkbytes / CD_FRAME_SIZE;
    disc->m_hunk.resize(int(header->hunkbytes));

    quint32 lba = 0, frame = 0;
    for (quint32 index = 0;; ++index) {
        char text[256] = {};
        quint32 length = 0;
        chd_error found = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, index, text, sizeof(text), &length, nullptr, nullptr);
        if (found != CHDERR_NONE)
            found = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, index, text, sizeof(text), &length, nullptr, nullptr);
        if (found != CHDERR_NONE)
            break;
        Track track;
        if (!parseTrack(text, &track)) {
            *error = "A track of this CHD cannot be read";
            return nullptr;
        }
        track.startLba = lba;
        track.firstFrame = frame;
        disc->m_tracks.append(track);
        lba += track.frames;
        frame += (track.frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING * CD_TRACK_PADDING;
    }
    if (disc->m_tracks.isEmpty()) {
        *error = "This CHD has no tracks";
        return nullptr;
    }
    return disc;
}

ChdDisc::~ChdDisc()
{
    if (m_chd)
        chd_close(m_chd);
    if (m_file)
        std::fclose(static_cast<FILE*>(m_file));
}

const ChdDisc::Track* ChdDisc::track(int number) const
{
    for (const Track& track : m_tracks)
        if (track.number == number)
            return &track;
    return nullptr;
}

const ChdDisc::Track* ChdDisc::firstDataTrack() const
{
    for (const Track& track : m_tracks)
        if (!track.audio)
            return &track;
    return nullptr;
}

bool ChdDisc::readFrame(quint32 frame, QByteArray* out)
{
    const quint32 hunk = frame / m_framesPerHunk;
    if (hunk != m_hunkNumber) {
        if (chd_read(m_chd, hunk, m_hunk.data()) != CHDERR_NONE) {
            m_hunkNumber = 0xFFFFFFFF;
            return false;
        }
        m_hunkNumber = hunk;
    }
    *out = m_hunk.mid(int(frame % m_framesPerHunk) * CD_FRAME_SIZE, CD_MAX_SECTOR_DATA);
    return true;
}

QByteArray ChdDisc::readTrack(const Track& track, quint32 sector, int bytes)
{
    QByteArray result;
    const int offset = userOffset(track.type);
    const int perSector = track.audio ? CD_MAX_SECTOR_DATA : 2048;
    while (result.size() < bytes && sector < track.frames) {
        QByteArray frame;
        if (!readFrame(track.firstFrame + sector, &frame))
            break;
        result.append(frame.mid(track.audio ? 0 : offset, perSector));
        ++sector;
    }
    result.truncate(bytes);
    return result;
}
