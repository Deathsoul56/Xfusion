#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include "Shortcuts.h"

// How the picture is fitted into the window.
enum class AspectMode {
    Core, // the aspect ratio the core reports for the system
    FourThree,
    Square, // square pixels: the frame's own width to height
};

// CPU filters applied to the frame before it is drawn; the same set as snes9xrd (see VideoFilters.h).
enum class VideoFilter {
    None,
    Scanlines,
    Simple2x,
    Simple3x,
    Simple4x,
    SuperEagle,
    TwoXSaI,
    Super2xSaI,
    EPX,
    HQ2x,
    HQ3x,
    HQ4x,
    XBRZ2x,
    XBRZ3x,
    XBRZ4x,
    XBRZ5x,
    XBRZ6x,
    SharpBilinear4x,
    NTSCComposite,
    NTSCRF,
    NTSCSVideo,
    NTSCRGB,
};

enum class MessageMode { Onscreen, None };
enum class OsdCorner { BottomLeft, BottomRight, TopLeft, TopRight };

// How the emulation is paced. SoundSync waits on the audio queue and is what the frontend always did.
enum class SpeedMethod { SoundSync, Timer, TimerFrameskip, Unlimited };

enum class FileLocation { DataFolder, RomFolder, Custom };

struct FolderSetting {
    FileLocation location = FileLocation::DataFolder;
    QString custom;
    bool operator==(const FolderSetting& other) const = default;
};

// Frontend settings that apply to every system and core (ADR-0013, ADR-0018). Video, audio and pacing are global
// because the frontend, not the core, draws, plays and times them; options that belong to one core live with its
// profile (ADR-0007).
struct AppSettings {
    static constexpr int kFileVersion = 3;
    static constexpr int kMaxVolume = 100;

    // General
    bool fullscreenOnOpen = false;
    bool pauseWhenUnfocused = true;
    bool backgroundGamepad = false;
    bool confirmSaveLoad = false;
    bool saveStateOnClose = false;
    int theme = 0;
    bool showFps = false;
    bool showIndicators = true;
    bool showPressedKeys = false;
    bool showTime = false;
    bool showRomInfo = true;
    int sramInterval = 30; // seconds; 0 saves only on close

    // Display
    AspectMode aspect = AspectMode::Core;
    bool maintainAspect = true;
    bool stretch = true; // off draws the picture at its own size, centred
    bool integerScale = false; // whole-number scale factors only, when the window is large enough
    bool smooth = true; // bilinear filtering; off keeps hard pixel edges
    VideoFilter filter = VideoFilter::None;
    MessageMode messages = MessageMode::Onscreen;
    int osdSize = 24; // points
    OsdCorner osdCorner = OsdCorner::BottomLeft;

    // Sound
    int volume = kMaxVolume; // 0..kMaxVolume
    int volumeFastForward = kMaxVolume;
    bool muted = false;
    bool muteDuringFastForward = false;
    bool muteDuringFrameAdvance = false;
    QString audioDevice; // SDL playback device name; empty is the system default
    int playbackRate = 0; // Hz; 0 plays at the core's own rate
    int bufferMs = 64;

    // Emulation
    SpeedMethod speedMethod = SpeedMethod::SoundSync;
    double fixedFrameRate = 0; // fps; 0 follows the core's own rate
    int maxFrameSkip = 0; // 0 is no limit
    int fastForwardSkip = 9; // frames skipped per drawn frame while fast-forwarding
    int fixedFrameSkip = 0;
    int rewindMegabytes = 10; // 0 turns rewind off
    int rewindInterval = 5; // frames between rewind snapshots
    // The core the user chose for a system (key of systemKey(), core id), when not the default of CoreRules.
    QMap<QString, QString> coreChoices;

    // Files
    FolderSetting sramFolder;
    FolderSetting stateFolder;
    FolderSetting screenshotFolder;
    FolderSetting biosFolder;
    QStringList libraryFolders; // searched for games, with their subfolders (the Library page)

    ShortcutMap shortcuts = ShortcutMap::defaults();

    // Linear gain for the audio stream: 0 when muted.
    float gain(bool fastForward = false) const { return muted ? 0.0f : float(fastForward ? volumeFastForward : volume) / kMaxVolume; }

    QJsonObject toJson() const;
    // Missing or invalid values keep their defaults; the volume is clamped. Files of version 1 keep their video
    // and volume settings; those of version 2 have their theme index translated to the current list.
    static AppSettings fromJson(const QJsonObject& object, QString* error = nullptr);
    // A missing file is not an error: the defaults are returned.
    static AppSettings load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    bool operator==(const AppSettings& other) const = default;
};
