#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Patches for game files: IPS, BPS and UPS. Cores do not apply them (libretro leaves that to the frontend), so the
// game is patched in memory and the core is given the result, as a temporary file; the original is never changed.
enum class PatchFormat { Ips, Bps, Ups };

struct PatchResult {
    bool ok = false;
    QByteArray data; // the patched game
    QString error; // what is wrong, in words for the user; empty when ok
    PatchFormat format = PatchFormat::Ips;
};

// The format a patch file is, from its first bytes ("PATCH"/"IPS32", "BPS1", "UPS1"); false when it is none.
bool patchFormatOf(const QByteArray& patch, PatchFormat* format);

// BPS and UPS carry the checksums of the game they are for and of the result, and those are checked: a patch for a
// different game is refused rather than applied. IPS has no checksums, so it is applied as it is.
PatchResult applyPatch(const QByteArray& game, const QByteArray& patch);

// The bytes of a game and the extension of the file they are in: the file itself, or the first game inside a zip. False
// (with `error`) when it cannot be read or the zip holds no game.
bool readGameBytes(const QString& path, QByteArray* data, QString* extension, QString* error);

// The patch for a game file: in each of `folders`, in order, a file with the name of the game (without its extension)
// and the extension .bps, .ups or .ips, in that order. Empty when there is none.
QString findPatch(const QString& gamePath, const QStringList& folders);
