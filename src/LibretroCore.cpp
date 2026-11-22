#include "LibretroCore.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "ZipRead.h"

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
    : m_profile(&profile)
    , m_options(profile.options)
{
    s_instance = this;
    setPads(PadType::SixButton, true);
    m_optionsChanged = false;
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

bool LibretroCore::useCore(const CoreProfile& profile, const QString& corePath, QString* error)
{
    if (m_loaded && m_profile == &profile && m_corePath == corePath)
        return true;
    unloadGame();
    if (m_loaded) {
        m_deinit();
        m_lib.unload();
        m_loaded = false;
    }
    m_profile = &profile;
    m_options = profile.options;
    m_usedOnce = false;
    m_name.clear();
    return load(corePath, error);
}

bool LibretroCore::load(const QString& corePath, QString* error)
{
    m_corePath = corePath;
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
    m_setControllerPortDevice = reinterpret_cast<decltype(m_setControllerPortDevice)>(m_lib.resolve("retro_set_controller_port_device"));
    m_serializeSize = reinterpret_cast<decltype(m_serializeSize)>(m_lib.resolve("retro_serialize_size"));
    m_serialize = reinterpret_cast<decltype(m_serialize)>(m_lib.resolve("retro_serialize"));
    m_unserialize = reinterpret_cast<decltype(m_unserialize)>(m_lib.resolve("retro_unserialize"));

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
    if (m_profile->reloadPerGame && m_usedOnce && !reloadCore(error))
        return false;
    m_usedOnce = true;
    unloadGame();

    const QString content = contentPath(gamePath, error);
    if (content.isEmpty())
        return false;
    m_gamePath = content.toUtf8();
    retro_game_info game{};
    game.path = m_gamePath.constData();
    if (!m_loadGame(&game)) {
        *error = QString("The core rejected '%1'. A BIOS may be missing or the file may be invalid.").arg(gamePath);
        return false;
    }
    m_hasGame = true;
    applyPortDevices();
    for (auto& pad : m_buttons)
        for (auto& button : pad)
            button = false;
    updateAvInfo();
    return true;
}

// Cores that do not open zip files get the game unpacked to a temporary file named like the game inside (the
// extension tells them what it is). SC-3000 games are SG-1000 ones to them. Everything else is passed as it is.
QString LibretroCore::contentPath(const QString& gamePath, QString* error)
{
    m_unpacked.reset();
    const QFileInfo info(gamePath);
    if (!m_profile->needsExtractedGames || info.suffix().compare("zip", Qt::CaseInsensitive) != 0)
        return gamePath;

    static const QStringList gameExtensions = {"md", "gen", "smd", "bin", "32x", "sms", "gg", "sg", "sc"};
    for (const ZipEntry& entry : zipEntries(gamePath)) {
        const QFileInfo inner(entry.name);
        if (!gameExtensions.contains(inner.suffix().toLower()))
            continue;
        const QByteArray bytes = zipReadHead(gamePath, entry, int(qMin<quint32>(entry.size, 64u * 1024 * 1024)));
        if (bytes.size() != qsizetype(entry.size)) {
            *error = QString("The game inside %1 could not be unpacked").arg(info.fileName());
            return {};
        }
        m_unpacked = std::make_unique<QTemporaryDir>();
        QString name = inner.fileName();
        if (inner.suffix().compare("sc", Qt::CaseInsensitive) == 0)
            name = inner.completeBaseName() + ".sg";
        QString target = m_unpacked->filePath(name);
        QFile file(target);
        if (!m_unpacked->isValid() || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            *error = "A temporary file for the game could not be written";
            m_unpacked.reset();
            return {};
        }
        return target;
    }
    *error = QString("There is no game inside %1").arg(info.fileName());
    return {};
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
    m_unpacked.reset();
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

void LibretroCore::setButton(int player, PadButton button, bool pressed)
{
    const unsigned retroId = m_profile->retroId(button);
    if (player >= 0 && player < kPlayers && retroId < m_buttons[0].size())
        m_buttons[static_cast<std::size_t>(player)][retroId].store(pressed, std::memory_order_relaxed);
}

void LibretroCore::setPads(PadType type, bool secondPadConnected, bool genesisFamily)
{
    for (std::size_t port = 0; port < m_profile->padTypeKeys.size(); ++port) {
        const bool empty = port > 0 && !secondPadConnected && !m_profile->noPadValue.empty();
        const std::string& wanted = empty ? m_profile->noPadValue : m_profile->padTypeValue(type);
        std::string& value = m_options[m_profile->padTypeKeys[port]];
        if (value != wanted) {
            value = wanted;
            m_optionsChanged = true;
        }
    }
    // Cores that take the pad as a device (Genesis Plus GX) get it now if a game is running, and when the next one loads.
    m_choosePortDevices = genesisFamily && m_profile->padDevices[0] != 0;
    m_portDevices[0] = m_profile->padDevices[static_cast<std::size_t>(type)];
    m_portDevices[1] = secondPadConnected ? m_portDevices[0] : RETRO_DEVICE_NONE;
    applyPortDevices();
}

void LibretroCore::setUserOptions(const std::map<std::string, std::string>& values)
{
    for (const CoreOption& option : m_profile->userOptions) {
        const auto wanted = values.find(option.key);
        if (wanted != values.end() && isValidCoreOption(option, wanted->second))
            m_options[option.key] = wanted->second;
        else
            m_options.erase(option.key);
    }
    m_optionsChanged = true;
}

void LibretroCore::applyPortDevices()
{
    if (!m_hasGame || !m_choosePortDevices || !m_setControllerPortDevice)
        return;
    for (unsigned port = 0; port < 2; ++port)
        m_setControllerPortDevice(port, m_portDevices[port]);
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

QByteArray LibretroCore::saveRam() const
{
    if (!m_hasGame)
        return {};
    const void* data = m_getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = m_getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0)
        return {};
    return QByteArray(static_cast<const char*>(data), static_cast<qsizetype>(size));
}

bool LibretroCore::setSaveRam(const QByteArray& bytes, QString* warning)
{
    void* data = m_getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = m_getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!m_hasGame || !data || size == 0) {
        *warning = "The core has no battery RAM for this game";
        return false;
    }
    // A longer file would be cut, so it is refused. A shorter one (other emulators store only the part a game
    // uses) is loaded at the start and the rest stays as the core cleared it.
    if (static_cast<size_t>(bytes.size()) > size)
        *warning = QString("The save file holds %1 bytes but the core expects %2").arg(bytes.size()).arg(size);
    std::memcpy(data, bytes.constData(), std::min(size, static_cast<size_t>(bytes.size())));
    return true;
}

bool LibretroCore::supportsStates() const
{
    return m_hasGame && m_serializeSize && m_serialize && m_unserialize && m_serializeSize() > 0;
}

bool LibretroCore::serialize(QByteArray* state, QString* error)
{
    if (!supportsStates()) {
        *error = "This core does not support save states";
        return false;
    }
    const size_t size = m_serializeSize();
    QByteArray buffer(static_cast<qsizetype>(size), ' ');
    if (!m_serialize(buffer.data(), size)) {
        *error = "The core could not save its state";
        return false;
    }
    *state = std::move(buffer);
    return true;
}

bool LibretroCore::unserialize(const QByteArray& state, QString* error)
{
    if (!supportsStates()) {
        *error = "This core does not support save states";
        return false;
    }
    if (!m_unserialize(state.constData(), static_cast<size_t>(state.size()))) {
        *error = "The core rejected the state";
        return false;
    }
    return true;
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
        const auto it = m_options.find(var->key);
        if (it == m_options.end())
            return false;
        var->value = it->second.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = m_optionsChanged;
        m_optionsChanged = false;
        return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        // Bit 0: render video, bit 1: produce audio.
        *static_cast<int*>(data) = m_skipVideo.load(std::memory_order_relaxed) ? 2 : 3;
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
    if (!s_instance || !data || !s_instance->m_video || s_instance->m_skipVideo.load(std::memory_order_relaxed))
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
    if (!s_instance || port >= unsigned(kPlayers) || device != RETRO_DEVICE_JOYPAD || id >= s_instance->m_buttons[0].size())
        return 0;
    return s_instance->m_buttons[port][id].load(std::memory_order_relaxed) ? 1 : 0;
}
