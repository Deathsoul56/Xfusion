#pragma once

#include <QString>
#include <QStringList>

#include "GameHeader.h"

// What can be learned about a Sega CD game from its files, without running it.
//
// Sega CD games come in three shapes: a .cue sheet that names one .bin per track (the cue is the game, the .bin
// files are its parts), a single .iso with the data track only (the audio then sits beside it as numbered .mp3,
// .wav, .ogg or .flac files), and a .chd. A bare .bin may be a cartridge or a raw disc track; the first sector
// tells which: a disc starts with "SEGADISCSYSTEM".
struct DiscInfo {
    bool isSegaCd = false; // the data track starts with "SEGADISCSYSTEM" (a .chd is taken as one, it cannot be looked into)
    QString format; // "ISO, 2048-byte sectors", "BIN/CUE, 2352-byte sectors", "CHD (not inspected)"
    QString dataFile; // the file that holds the data track
    QString volume; // the disc's volume name
    GameHeader header; // the game's header, from the first sector
    qint64 totalSize = 0; // all the files of the disc together: the data track and the audio
    int dataTracks = 0;
    int audioTracks = 0;
    QStringList audioFiles;
    // What is wrong with the files of the disc: a track the cue names that is not there, a data track that cannot be
    // read. Empty when the disc is complete as far as can be told.
    QStringList problems;
};

// `path` is a .cue, .iso, .bin or .chd file. A file that is not a Sega CD disc gives `isSegaCd == false`.
DiscInfo readDisc(const QString& path);
