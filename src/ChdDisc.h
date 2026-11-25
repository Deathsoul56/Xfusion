#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <memory>

struct _chd_file;

// A CD image in the CHD format, read with libchdr (ADR-0029): its tracks and the data of a track sector by sector. Only
// what the frontend needs to name a disc and to let RetroAchievements identify it.
class ChdDisc {
public:
    struct Track {
        int number = 0;
        QString type; // "MODE1", "MODE1_RAW", "MODE2_RAW", "AUDIO", ...
        bool audio = false;
        quint32 frames = 0; // sectors of the track, with its pregap when the pregap is stored
        quint32 pregap = 0;
        quint32 startLba = 0; // where the track starts on the disc
        quint32 firstFrame = 0; // where its frames start in the image, which pads each track to a multiple of four
    };

    // Null, with `error` set, when the file is not a CHD of a CD that libchdr can read.
    static std::unique_ptr<ChdDisc> open(const QString& path, QString* error);
    ~ChdDisc();
    ChdDisc(const ChdDisc&) = delete;
    ChdDisc& operator=(const ChdDisc&) = delete;

    const QVector<Track>& tracks() const { return m_tracks; }
    const Track* track(int number) const;
    // The first track that is not audio, or null.
    const Track* firstDataTrack() const;
    // Up to `bytes` bytes of the user data (2048 per sector) of a track from its sector `sector` (counted from the
    // start of the track), going on into the next sectors. Shorter, or empty, when the track ends or a hunk fails.
    QByteArray readTrack(const Track& track, quint32 sector, int bytes);

private:
    ChdDisc() = default;
    bool readFrame(quint32 frame, QByteArray* out);

    _chd_file* m_chd = nullptr;
    void* m_file = nullptr; // the FILE the image is read from
    QVector<Track> m_tracks;
    QByteArray m_hunk;
    quint32 m_hunkNumber = 0xFFFFFFFF;
    quint32 m_framesPerHunk = 0;
};
