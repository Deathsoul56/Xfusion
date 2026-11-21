#include "AppSettings.h"

#include "VideoFilters.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <vector>

namespace {

template <class E>
struct Names {
    std::vector<std::pair<E, const char*>> items;

    QString name(E value) const
    {
        for (const auto& [e, text] : items)
            if (e == value)
                return text;
        return items.begin()->second;
    }
    E value(const QString& text, E fallback) const
    {
        for (const auto& [e, name] : items)
            if (text == name)
                return e;
        return fallback;
    }
};

const Names<AspectMode> kAspect{{{AspectMode::Core, "core"}, {AspectMode::FourThree, "4:3"}, {AspectMode::Square, "square"}}};
const Names<MessageMode> kMessages{{{MessageMode::Onscreen, "onscreen"}, {MessageMode::None, "none"}}};
const Names<OsdCorner> kCorner{{{OsdCorner::BottomLeft, "bottom_left"}, {OsdCorner::BottomRight, "bottom_right"},
                                {OsdCorner::TopLeft, "top_left"}, {OsdCorner::TopRight, "top_right"}}};
const Names<SpeedMethod> kSpeed{{{SpeedMethod::SoundSync, "sound_sync"}, {SpeedMethod::Timer, "timer"},
                                 {SpeedMethod::TimerFrameskip, "timer_frameskip"}, {SpeedMethod::Unlimited, "unlimited"}}};
const Names<FileLocation> kLocation{{{FileLocation::DataFolder, "data"}, {FileLocation::RomFolder, "rom"}, {FileLocation::Custom, "custom"}}};

QJsonObject folderToJson(const FolderSetting& folder)
{
    return {{"location", kLocation.name(folder.location)}, {"custom", folder.custom}};
}

FolderSetting folderFromJson(const QJsonObject& object)
{
    FolderSetting folder;
    folder.location = kLocation.value(object.value("location").toString(), folder.location);
    folder.custom = object.value("custom").toString();
    return folder;
}

}

QJsonObject AppSettings::toJson() const
{
    return {
        {"version", kFileVersion},
        {"general",
         QJsonObject{{"fullscreen_on_open", fullscreenOnOpen},
                     {"pause_when_unfocused", pauseWhenUnfocused},
                     {"background_gamepad", backgroundGamepad},
                     {"confirm_save_load", confirmSaveLoad},
                     {"save_state_on_close", saveStateOnClose},
                     {"theme", theme},
                     {"show_fps", showFps},
                     {"show_indicators", showIndicators},
                     {"show_pressed_keys", showPressedKeys},
                     {"show_time", showTime},
                     {"show_rom_info", showRomInfo},
                     {"sram_interval", sramInterval}}},
        {"display",
         QJsonObject{{"aspect", kAspect.name(aspect)},
                     {"maintain_aspect", maintainAspect},
                     {"stretch", stretch},
                     {"integer_scale", integerScale},
                     {"smooth", smooth},
                     {"filter", videoFilterKey(filter)},
                     {"messages", kMessages.name(messages)},
                     {"osd_size", osdSize},
                     {"osd_corner", kCorner.name(osdCorner)}}},
        {"sound",
         QJsonObject{{"volume", volume},
                     {"volume_fast_forward", volumeFastForward},
                     {"muted", muted},
                     {"mute_during_fast_forward", muteDuringFastForward},
                     {"mute_during_frame_advance", muteDuringFrameAdvance},
                     {"device", audioDevice},
                     {"playback_rate", playbackRate},
                     {"buffer_ms", bufferMs}}},
        {"emulation",
         QJsonObject{{"speed_method", kSpeed.name(speedMethod)},
                     {"fixed_frame_rate", fixedFrameRate},
                     {"max_frame_skip", maxFrameSkip},
                     {"fast_forward_skip", fastForwardSkip},
                     {"fixed_frame_skip", fixedFrameSkip},
                     {"rewind_megabytes", rewindMegabytes},
                     {"rewind_interval", rewindInterval}}},
        {"files",
         QJsonObject{{"sram", folderToJson(sramFolder)},
                     {"states", folderToJson(stateFolder)},
                     {"screenshots", folderToJson(screenshotFolder)},
                     {"bios", folderToJson(biosFolder)},
                     {"library_folders", QJsonArray::fromStringList(libraryFolders)}}},
        {"shortcuts", shortcuts.toJson()},
    };
}

AppSettings AppSettings::fromJson(const QJsonObject& object, QString* error)
{
    AppSettings s;
    const int version = object.value("version").toInt();
    if (version < 1 || version > kFileVersion) {
        if (error)
            *error = QString("Unsupported settings version %1").arg(version);
        return s;
    }

    if (version == 1) {
        // Version 1 kept a few video and sound values at the top level; "stretch" was an aspect mode.
        const QString aspectName = object.value("aspect").toString();
        s.maintainAspect = aspectName != "stretch";
        s.aspect = kAspect.value(aspectName, AspectMode::Core);
        s.smooth = object.value("smooth").toBool(false);
        s.integerScale = object.value("integer_scale").toBool(s.integerScale);
        s.volume = std::clamp(object.value("volume").toInt(s.volume), 0, kMaxVolume);
        s.muted = object.value("muted").toBool(s.muted);
        return s;
    }

    const QJsonObject general = object.value("general").toObject();
    s.fullscreenOnOpen = general.value("fullscreen_on_open").toBool(s.fullscreenOnOpen);
    s.pauseWhenUnfocused = general.value("pause_when_unfocused").toBool(s.pauseWhenUnfocused);
    s.backgroundGamepad = general.value("background_gamepad").toBool(s.backgroundGamepad);
    s.confirmSaveLoad = general.value("confirm_save_load").toBool(s.confirmSaveLoad);
    s.saveStateOnClose = general.value("save_state_on_close").toBool(s.saveStateOnClose);
    s.theme = std::clamp(general.value("theme").toInt(s.theme), 0, 99);
    if (version == 2) {
        // Version 2 had seven themes of its own; the list is now the one of snes9xrd.
        static constexpr int kVersion2Themes[] = {0, 1, 8, 2, 3, 4, 17, 18};
        s.theme = s.theme < 8 ? kVersion2Themes[s.theme] : 0;
    }
    s.showFps = general.value("show_fps").toBool(s.showFps);
    s.showIndicators = general.value("show_indicators").toBool(s.showIndicators);
    s.showPressedKeys = general.value("show_pressed_keys").toBool(s.showPressedKeys);
    s.showTime = general.value("show_time").toBool(s.showTime);
    s.showRomInfo = general.value("show_rom_info").toBool(s.showRomInfo);
    s.sramInterval = std::clamp(general.value("sram_interval").toInt(s.sramInterval), 0, 1000);

    const QJsonObject display = object.value("display").toObject();
    s.aspect = kAspect.value(display.value("aspect").toString(), s.aspect);
    s.maintainAspect = display.value("maintain_aspect").toBool(s.maintainAspect);
    s.stretch = display.value("stretch").toBool(s.stretch);
    s.integerScale = display.value("integer_scale").toBool(s.integerScale);
    s.smooth = display.value("smooth").toBool(s.smooth);
    s.filter = videoFilterFromKey(display.value("filter").toString(), s.filter);
    s.messages = kMessages.value(display.value("messages").toString(), s.messages);
    s.osdSize = std::clamp(display.value("osd_size").toInt(s.osdSize), 8, 256);
    s.osdCorner = kCorner.value(display.value("osd_corner").toString(), s.osdCorner);

    const QJsonObject sound = object.value("sound").toObject();
    s.volume = std::clamp(sound.value("volume").toInt(s.volume), 0, kMaxVolume);
    s.volumeFastForward = std::clamp(sound.value("volume_fast_forward").toInt(s.volumeFastForward), 0, kMaxVolume);
    s.muted = sound.value("muted").toBool(s.muted);
    s.muteDuringFastForward = sound.value("mute_during_fast_forward").toBool(s.muteDuringFastForward);
    s.muteDuringFrameAdvance = sound.value("mute_during_frame_advance").toBool(s.muteDuringFrameAdvance);
    s.audioDevice = sound.value("device").toString();
    s.playbackRate = std::clamp(sound.value("playback_rate").toInt(s.playbackRate), 0, 192000);
    s.bufferMs = std::clamp(sound.value("buffer_ms").toInt(s.bufferMs), 16, 256);

    const QJsonObject emulation = object.value("emulation").toObject();
    s.speedMethod = kSpeed.value(emulation.value("speed_method").toString(), s.speedMethod);
    s.fixedFrameRate = std::clamp(emulation.value("fixed_frame_rate").toDouble(s.fixedFrameRate), 0.0, 1000.0);
    s.maxFrameSkip = std::clamp(emulation.value("max_frame_skip").toInt(s.maxFrameSkip), 0, 59);
    s.fastForwardSkip = std::clamp(emulation.value("fast_forward_skip").toInt(s.fastForwardSkip), 1, 15);
    s.fixedFrameSkip = std::clamp(emulation.value("fixed_frame_skip").toInt(s.fixedFrameSkip), 0, 59);
    s.rewindMegabytes = std::clamp(emulation.value("rewind_megabytes").toInt(s.rewindMegabytes), 0, 1024);
    s.rewindInterval = std::clamp(emulation.value("rewind_interval").toInt(s.rewindInterval), 1, 99);

    const QJsonObject files = object.value("files").toObject();
    s.sramFolder = folderFromJson(files.value("sram").toObject());
    s.stateFolder = folderFromJson(files.value("states").toObject());
    s.screenshotFolder = folderFromJson(files.value("screenshots").toObject());
    s.biosFolder = folderFromJson(files.value("bios").toObject());
    for (const QJsonValue& folder : files.value("library_folders").toArray())
        if (folder.isString() && !folder.toString().isEmpty() && !s.libraryFolders.contains(folder.toString()))
            s.libraryFolders << folder.toString();

    s.shortcuts = ShortcutMap::fromJson(object.value("shortcuts").toObject());
    return s;
}

AppSettings AppSettings::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString("Cannot read %1: %2").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error)
            *error = QString("Invalid settings file %1: %2").arg(path, parseError.errorString());
        return {};
    }
    return fromJson(document.object(), error);
}

bool AppSettings::save(const QString& path, QString* error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
