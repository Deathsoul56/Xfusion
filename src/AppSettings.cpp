#include "AppSettings.h"

#include "CoreProfile.h"
#include "CoreRules.h"
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
const Names<VideoShader> kShader{{{VideoShader::None, "none"}, {VideoShader::Scanlines, "scanlines"}, {VideoShader::Crt, "crt"},
                                 {VideoShader::SharpBilinear, "sharp_bilinear"}, {VideoShader::Lcd, "lcd"}, {VideoShader::Preset, "preset"}}};
const Names<VideoDriver> kDriver{{{VideoDriver::Auto, "auto"}, {VideoDriver::Software, "software"}, {VideoDriver::Direct3D11, "d3d11"},
                                  {VideoDriver::Direct3D12, "d3d12"}, {VideoDriver::Vulkan, "vulkan"}, {VideoDriver::OpenGL, "opengl"}}};
const Names<MessageMode> kMessages{{{MessageMode::Onscreen, "onscreen"}, {MessageMode::None, "none"}}};
const Names<OsdCorner> kCorner{{{OsdCorner::BottomLeft, "bottom_left"}, {OsdCorner::BottomRight, "bottom_right"},
                                {OsdCorner::TopLeft, "top_left"}, {OsdCorner::TopRight, "top_right"}}};
const Names<SpeedMethod> kSpeed{{{SpeedMethod::SoundSync, "sound_sync"}, {SpeedMethod::Timer, "timer"},
                                 {SpeedMethod::TimerFrameskip, "timer_frameskip"}, {SpeedMethod::Unlimited, "unlimited"}}};
const Names<FileLocation> kLocation{{{FileLocation::DataFolder, "data"}, {FileLocation::RomFolder, "rom"}, {FileLocation::Custom, "custom"}}};

QJsonObject coreChoicesToJson(const QMap<QString, QString>& choices)
{
    QJsonObject object;
    for (auto it = choices.begin(); it != choices.end(); ++it)
        object[it.key()] = it.value();
    return object;
}

QJsonObject coreOptionsToJson(const QMap<QString, QMap<QString, QString>>& options)
{
    QJsonObject object;
    for (auto core = options.begin(); core != options.end(); ++core)
        object[core.key()] = coreChoicesToJson(core.value());
    return object;
}

QJsonObject folderToJson(const FolderSetting& folder)
{
    return {{"location", kLocation.name(folder.location)}, {"custom", folder.custom}};
}

QJsonObject shaderParametersToJson(const QMap<QString, QMap<QString, double>>& parameters)
{
    QJsonObject object;
    for (auto preset = parameters.begin(); preset != parameters.end(); ++preset) {
        QJsonObject values;
        for (auto it = preset.value().begin(); it != preset.value().end(); ++it)
            values[it.key()] = it.value();
        if (!values.isEmpty())
            object[preset.key()] = values;
    }
    return object;
}

QJsonObject systemOverridesToJson(const QMap<QString, QJsonObject>& overrides)
{
    QJsonObject object;
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        if (!it.value().isEmpty())
            object[it.key()] = it.value();
    return object;
}

FolderSetting folderFromJson(const QJsonObject& object)
{
    FolderSetting folder;
    folder.location = kLocation.value(object.value("location").toString(), folder.location);
    folder.custom = object.value("custom").toString();
    return folder;
}

}

const QStringList& AppSettings::overridableKeys(const QString& section)
{
    static const QStringList display{"aspect", "maintain_aspect", "stretch", "integer_scale", "smooth", "filter", "gpu_filters", "shader", "shader_preset", "messages", "osd_size", "osd_corner"};
    static const QStringList sound{"volume", "volume_fast_forward", "muted", "mute_during_fast_forward", "mute_during_frame_advance", "device", "playback_rate", "buffer_ms"};
    static const QStringList none;
    return section == "display" ? display : section == "sound" ? sound : none;
}

AppSettings AppSettings::forSystem(System system) const
{
    const auto found = systemOverrides.constFind(systemKey(system));
    if (found == systemOverrides.constEnd() || found->isEmpty())
        return *this;
    // The overrides are laid over the file form of the global settings and read back, so that they get the same
    // validation (clamping, unknown values) as any other value.
    QJsonObject json = toJson();
    for (const QString& section : {QString("display"), QString("sound")}) {
        QJsonObject merged = json.value(section).toObject();
        const QJsonObject over = found->value(section).toObject();
        for (const QString& key : overridableKeys(section))
            if (over.contains(key))
                merged[key] = over.value(key);
        json[section] = merged;
    }
    AppSettings result = fromJson(json);
    result.systemOverrides = systemOverrides;
    return result;
}

bool AppSettings::overrides(System system, const QString& section, const QString& key) const
{
    return systemOverrides.value(systemKey(system)).value(section).toObject().contains(key);
}

void AppSettings::setSoundValue(std::optional<System> system, const QString& key, const QJsonValue& value)
{
    if (system && overrides(*system, "sound", key)) {
        QJsonObject entry = systemOverrides.value(systemKey(*system));
        QJsonObject sound = entry.value("sound").toObject();
        sound[key] = value;
        entry["sound"] = sound;
        systemOverrides[systemKey(*system)] = entry;
    } else if (key == "volume") {
        volume = std::clamp(value.toInt(volume), 0, kMaxVolume);
    } else if (key == "muted") {
        muted = value.toBool(muted);
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
                     {"gpu_filters", gpuFilters},
                     {"shader", kShader.name(shader)},
                     {"shader_preset", shaderPreset},
                     {"shader_parameters", shaderParametersToJson(shaderParameters)},
                     {"driver", kDriver.name(videoDriver)},
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
                     {"rewind_interval", rewindInterval},
                     {"cores", coreChoicesToJson(coreChoices)},
                                {"core_options", coreOptionsToJson(coreOptions)}}},
        {"files",
         QJsonObject{{"sram", folderToJson(sramFolder)},
                     {"states", folderToJson(stateFolder)},
                     {"screenshots", folderToJson(screenshotFolder)},
                     {"bios", folderToJson(biosFolder)},
                     {"patches", folderToJson(patchFolder)},
                     {"library_folders", QJsonArray::fromStringList(libraryFolders)}}},
        {"achievements",
         QJsonObject{{"enabled", achievements.enabled},
                     {"username", achievements.username},
                     {"token", achievements.token},
                     {"spectator", achievements.spectator},
                     {"encore", achievements.encore},
                     {"unofficial", achievements.unofficial},
                     {"notifications", achievements.notifications},
                     {"leaderboard_notifications", achievements.leaderboardNotifications},
                     {"leaderboard_trackers", achievements.leaderboardTrackers},
                     {"progress_indicators", achievements.progressIndicators},
                     {"challenge_indicators", achievements.challengeIndicators},
                     {"duration_seconds", achievements.durationSeconds},
                     {"unlock_sound", achievements.unlockSound}}},
        {"shortcuts", shortcuts.toJson()},
        {"systems", systemOverridesToJson(systemOverrides)},
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
        s.stretch = aspectName == "stretch";
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
    if (version <= 3) {
        // Until version 3, "stretch" off drew the picture at its own size and on fitted it; filling the whole area was
        // "maintain aspect" off. Now "stretch" is the one that fills, and nothing draws the picture at its own size.
        s.stretch = display.value("stretch").toBool(true) && !s.maintainAspect;
    }
    s.integerScale = display.value("integer_scale").toBool(s.integerScale);
    s.smooth = display.value("smooth").toBool(s.smooth);
    s.filter = videoFilterFromKey(display.value("filter").toString(), s.filter);
    s.gpuFilters = display.value("gpu_filters").toBool(s.gpuFilters);
    s.shader = kShader.value(display.value("shader").toString(), s.shader);
    s.shaderPreset = display.value("shader_preset").toString();
    const QJsonObject parameters = display.value("shader_parameters").toObject();
    for (auto preset = parameters.begin(); preset != parameters.end(); ++preset) {
        const QJsonObject values = preset.value().toObject();
        for (auto it = values.begin(); it != values.end(); ++it)
            if (it.value().isDouble())
                s.shaderParameters[preset.key()][it.key()] = it.value().toDouble();
    }
    s.videoDriver = kDriver.value(display.value("driver").toString(), s.videoDriver);
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
    const QJsonObject cores = emulation.value("cores").toObject();
    for (auto it = cores.begin(); it != cores.end(); ++it)
        if (it.value().isString() && coreInfo(it.value().toString()))
            s.coreChoices[it.key()] = it.value().toString();
    // Only what the profile of the core lists, with a value it lists, so that a stale or edited file cannot set an option the core was not meant to get.
    const QJsonObject coreOptions = emulation.value("core_options").toObject();
    for (auto core = coreOptions.begin(); core != coreOptions.end(); ++core) {
        const CoreInfo* info = coreInfo(core.key());
        if (!info)
            continue;
        const QJsonObject values = core.value().toObject();
        for (auto it = values.begin(); it != values.end(); ++it) {
            const CoreOption* option = coreProfileById(info->id).userOption(it.key().toStdString());
            if (option && it.value().isString() && isValidCoreOption(*option, it.value().toString().toStdString()))
                s.coreOptions[info->id][it.key()] = it.value().toString();
        }
    }

    const QJsonObject files = object.value("files").toObject();
    s.sramFolder = folderFromJson(files.value("sram").toObject());
    s.stateFolder = folderFromJson(files.value("states").toObject());
    s.screenshotFolder = folderFromJson(files.value("screenshots").toObject());
    s.biosFolder = folderFromJson(files.value("bios").toObject());
    s.patchFolder = folderFromJson(files.value("patches").toObject());
    for (const QJsonValue folder : files.value("library_folders").toArray())
        if (folder.isString() && !folder.toString().isEmpty() && !s.libraryFolders.contains(folder.toString()))
            s.libraryFolders << folder.toString();

    const QJsonObject achievements = object.value("achievements").toObject();
    AchievementsSettings& a = s.achievements;
    a.enabled = achievements.value("enabled").toBool(a.enabled);
    a.username = achievements.value("username").toString();
    a.token = achievements.value("token").toString();
    a.spectator = achievements.value("spectator").toBool(a.spectator);
    a.encore = achievements.value("encore").toBool(a.encore);
    a.unofficial = achievements.value("unofficial").toBool(a.unofficial);
    a.notifications = achievements.value("notifications").toBool(a.notifications);
    a.leaderboardNotifications = achievements.value("leaderboard_notifications").toBool(a.leaderboardNotifications);
    a.leaderboardTrackers = achievements.value("leaderboard_trackers").toBool(a.leaderboardTrackers);
    a.progressIndicators = achievements.value("progress_indicators").toBool(a.progressIndicators);
    a.challengeIndicators = achievements.value("challenge_indicators").toBool(a.challengeIndicators);
    a.durationSeconds = std::clamp(achievements.value("duration_seconds").toInt(a.durationSeconds), 1, 30);
    a.unlockSound = achievements.value("unlock_sound").toBool(a.unlockSound);
    // A saved account needs both halves.
    if (a.username.isEmpty() || a.token.isEmpty())
        a.username.clear(), a.token.clear();

    // Only the systems that exist and, in their display and sound sections, only the keys that can be overridden.
    const QJsonObject systems = object.value("systems").toObject();
    for (const SystemDef& def : allSystems()) {
        const QJsonObject entry = systems.value(systemKey(def.id)).toObject();
        QJsonObject kept;
        for (const QString& section : {QString("display"), QString("sound")}) {
            const QJsonObject source = entry.value(section).toObject();
            QJsonObject values;
            for (const QString& key : overridableKeys(section))
                if (source.contains(key) && !source.value(key).isNull())
                    values[key] = source.value(key);
            if (!values.isEmpty())
                kept[section] = values;
        }
        if (!kept.isEmpty())
            s.systemOverrides[systemKey(def.id)] = kept;
    }

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
