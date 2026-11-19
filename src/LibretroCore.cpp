#include "LibretroCore.h"

#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

LibretroCore* LibretroCore::s_instance = nullptr;

namespace {

void logCb(enum retro_log_level level, const char* fmt, ...)
{
    if (level < RETRO_LOG_INFO)
        return;
    std::va_list args;
    va_start(args, fmt);
    std::fputs("[core] ", stderr);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
}

}

LibretroCore::LibretroCore(const CoreProfile& profile)
    : m_profile(profile)
{
    s_instance = this;
}

LibretroCore::~LibretroCore()
{
    unloadGame();
    if (m_loaded) {
        m_deinit();
        m_lib.unload();
    }
    s_instance = nullptr;
}

bool LibretroCore::load(const QString& corePath, QString* error)
{
    m_lib.setFileName(corePath);
    if (!m_lib.load()) {
        *error = QString("Cannot load core '%1': %2").arg(corePath, m_lib.errorString());
        return false;
    }

    bool ok = true;
    auto resolve = [&](auto& fn, const char* symbol) {
        fn = reinterpret_cast<std::decay_t<decltype(fn)>>(m_lib.resolve(symbol));
        if (!fn) {
            *error = QString("The core does not export '%1'").arg(symbol);
            ok = false;
        }
    };
    resolve(m_setEnvironment, "retro_set_environment");
    resolve(m_setVideoRefresh, "retro_set_video_refresh");
    resolve(m_setAudioSample, "retro_set_audio_sample");
    resolve(m_setAudioSampleBatch, "retro_set_audio_sample_batch");
    resolve(m_setInputPoll, "retro_set_input_poll");
    resolve(m_setInputState, "retro_set_input_state");
    resolve(m_init, "retro_init");
    resolve(m_deinit, "retro_deinit");
    resolve(m_apiVersion, "retro_api_version");
    resolve(m_getSystemInfo, "retro_get_system_info");
    resolve(m_getSystemAvInfo, "retro_get_system_av_info");
    resolve(m_loadGame, "retro_load_game");
    resolve(m_unloadGame, "retro_unload_game");
    resolve(m_run, "retro_run");
    resolve(m_reset, "retro_reset");
    resolve(m_getMemoryData, "retro_get_memory_data");
    resolve(m_getMemorySize, "retro_get_memory_size");
    if (!ok) {
        m_lib.unload();
        return false;
    }

    if (m_apiVersion() != RETRO_API_VERSION) {
        *error = QString("Unsupported libretro API version %1").arg(m_apiVersion());
        m_lib.unload();
        return false;
    }

    m_setEnvironment(environmentCb);
    m_setVideoRefresh(videoCb);
    m_setAudioSample(audioSampleCb);
    m_setAudioSampleBatch(audioBatchCb);
    m_setInputPoll(inputPollCb);
    m_setInputState(inputStateCb);
    m_init();

    retro_system_info info{};
    m_getSystemInfo(&info);
    m_name = QString("%1 %2").arg(info.library_name, info.library_version);
    m_loaded = true;
    return true;
}

void LibretroCore::setDirectories(const QString& systemDir, const QString& saveDir)
{
    m_systemDir = systemDir.toUtf8();
    m_saveDir = saveDir.toUtf8();
}

bool LibretroCore::loadGame(const QString& gamePath, QString* error)
{
    if (!m_loaded) {
        *error = "No core is loaded";
        return false;
    }
    if (m_profile.reloadPerGame && m_usedOnce && !reloadCore(error))
        return false;
    m_usedOnce = true;
    unloadGame();

    m_gamePath = gamePath.toUtf8();
    retro_game_info game{};
    game.path = m_gamePath.constData();
    if (!m_loadGame(&game)) {
        *error = QString("The core rejected '%1'. A BIOS may be missing or the file may be invalid.").arg(gamePath);
        return false;
    }
    m_hasGame = true;
    for (auto& button : m_buttons)
        button = false;
    updateAvInfo();
    return true;
}

bool LibretroCore::reloadCore(QString* error)
{
    const QString path = m_lib.fileName();
    unloadGame();
    m_deinit();
    m_lib.unload();
    m_loaded = false;
    return load(path, error);
}

void LibretroCore::unloadGame()
{
    if (!m_hasGame)
        return;
    m_unloadGame();
    m_hasGame = false;
}

void LibretroCore::run()
{
    if (m_hasGame)
        m_run();
}

void LibretroCore::reset()
{
    if (m_hasGame)
        m_reset();
}

void LibretroCore::setButton(PadButton button, bool pressed)
{
    const unsigned retroId = m_profile.retroId(button);
    if (retroId < m_buttons.size())
        m_buttons[retroId].store(pressed, std::memory_order_relaxed);
}

void LibretroCore::updateAvInfo()
{
    retro_system_av_info av{};
    m_getSystemAvInfo(&av);
    m_av.fps = av.timing.fps > 0 ? av.timing.fps : 60.0;
    m_av.sampleRate = av.timing.sample_rate > 0 ? av.timing.sample_rate : 44100.0;
    m_av.aspectRatio = av.geometry.aspect_ratio > 0
        ? av.geometry.aspect_ratio
        : double(av.geometry.base_width) / std::max(1u, av.geometry.base_height);
}

bool LibretroCore::readSaveRam(const QString& path)
{
    void* data = m_getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = m_getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.read(static_cast<qint64>(size));
    std::memcpy(data, bytes.constData(), bytes.size());
    return true;
}

bool LibretroCore::writeSaveRam(const QString& path)
{
    const void* data = m_getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = m_getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0)
        return true;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(static_cast<const char*>(data), static_cast<qint64>(size));
    return file.commit();
}

bool LibretroCore::environmentCb(unsigned cmd, void* data)
{
    return s_instance && s_instance->handleEnvironment(cmd, data);
}

bool LibretroCore::handleEnvironment(unsigned cmd, void* data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const auto format = *static_cast<retro_pixel_format*>(data);
        if (format != RETRO_PIXEL_FORMAT_RGB565 && format != RETRO_PIXEL_FORMAT_XRGB8888)
            return false;
        m_format = format;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char**>(data) = m_systemDir.constData();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = m_saveDir.constData();
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* var = static_cast<retro_variable*>(data);
        if (!var->key)
            return false;
        const auto it = m_profile.options.find(var->key);
        if (it == m_profile.options.end())
            return false;
        var->value = it->second.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = logCb;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        if (m_hasGame)
            updateAvInfo();
        return true;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
        return true;
    default:
        return false;
    }
}

void LibretroCore::videoCb(const void* data, unsigned width, unsigned height, size_t pitch)
{
    if (!s_instance || !data || !s_instance->m_video)
        return;
    const QImage::Format format = s_instance->m_format == RETRO_PIXEL_FORMAT_RGB565
        ? QImage::Format_RGB16
        : QImage::Format_RGB32;
    const QImage frame(static_cast<const uchar*>(data), int(width), int(height), qsizetype(pitch), format);
    s_instance->m_video(frame, s_instance->m_av.aspectRatio);
}

void LibretroCore::audioSampleCb(int16_t left, int16_t right)
{
    const int16_t frame[2] = {left, right};
    audioBatchCb(frame, 1);
}

size_t LibretroCore::audioBatchCb(const int16_t* data, size_t frames)
{
    if (s_instance && s_instance->m_audio)
        s_instance->m_audio(data, frames);
    return frames;
}

void LibretroCore::inputPollCb()
{
}

int16_t LibretroCore::inputStateCb(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (!s_instance || port != 0 || device != RETRO_DEVICE_JOYPAD || id >= s_instance->m_buttons.size())
        return 0;
    return s_instance->m_buttons[id].load(std::memory_order_relaxed) ? 1 : 0;
}
