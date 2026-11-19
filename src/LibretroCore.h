#pragma once

#include <QImage>
#include <QLibrary>
#include <QString>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>

#include "CoreProfile.h"
#include "libretro.h"

class LibretroCore {
public:
    struct AvInfo {
        double fps = 60.0;
        double sampleRate = 44100.0;
        double aspectRatio = 4.0 / 3.0;
    };

    // The image wraps core memory and is only valid during the call; copy it to keep it.
    using VideoCallback = std::function<void(const QImage&, double aspectRatio)>;
    using AudioCallback = std::function<void(const int16_t*, size_t frames)>;

    LibretroCore(const CoreProfile& profile = defaultCoreProfile());
    ~LibretroCore();
    LibretroCore(const LibretroCore&) = delete;
    LibretroCore& operator=(const LibretroCore&) = delete;

    bool load(const QString& corePath, QString* error);
    bool loadGame(const QString& gamePath, QString* error);
    void unloadGame();
    void run();
    void reset();

    bool isLoaded() const { return m_loaded; }
    bool hasGame() const { return m_hasGame; }
    const AvInfo& avInfo() const { return m_av; }
    const QString& name() const { return m_name; }

    void setDirectories(const QString& systemDir, const QString& saveDir);
    void setVideoCallback(VideoCallback cb) { m_video = std::move(cb); }
    void setAudioCallback(AudioCallback cb) { m_audio = std::move(cb); }
    void setButton(PadButton button, bool pressed);
    const CoreProfile& profile() const { return m_profile; }

    bool readSaveRam(const QString& path);
    bool writeSaveRam(const QString& path);

private:
    static LibretroCore* s_instance;
    static bool environmentCb(unsigned cmd, void* data);
    static void videoCb(const void* data, unsigned width, unsigned height, size_t pitch);
    static void audioSampleCb(int16_t left, int16_t right);
    static size_t audioBatchCb(const int16_t* data, size_t frames);
    static void inputPollCb();
    static int16_t inputStateCb(unsigned port, unsigned device, unsigned index, unsigned id);

    bool handleEnvironment(unsigned cmd, void* data);
    void updateAvInfo();
    bool reloadCore(QString* error);

    const CoreProfile& m_profile;
    QLibrary m_lib;
    bool m_loaded = false;
    bool m_hasGame = false;
    bool m_usedOnce = false;
    QString m_name;
    AvInfo m_av;
    retro_pixel_format m_format = RETRO_PIXEL_FORMAT_0RGB1555;
    QByteArray m_systemDir;
    QByteArray m_saveDir;
    QByteArray m_gamePath;
    std::array<std::atomic<bool>, 16> m_buttons{};
    VideoCallback m_video;
    AudioCallback m_audio;

    void (*m_setEnvironment)(retro_environment_t) = nullptr;
    void (*m_setVideoRefresh)(retro_video_refresh_t) = nullptr;
    void (*m_setAudioSample)(retro_audio_sample_t) = nullptr;
    void (*m_setAudioSampleBatch)(retro_audio_sample_batch_t) = nullptr;
    void (*m_setInputPoll)(retro_input_poll_t) = nullptr;
    void (*m_setInputState)(retro_input_state_t) = nullptr;
    void (*m_init)() = nullptr;
    void (*m_deinit)() = nullptr;
    unsigned (*m_apiVersion)() = nullptr;
    void (*m_getSystemInfo)(retro_system_info*) = nullptr;
    void (*m_getSystemAvInfo)(retro_system_av_info*) = nullptr;
    bool (*m_loadGame)(const retro_game_info*) = nullptr;
    void (*m_unloadGame)() = nullptr;
    void (*m_run)() = nullptr;
    void (*m_reset)() = nullptr;
    void* (*m_getMemoryData)(unsigned) = nullptr;
    size_t (*m_getMemorySize)(unsigned) = nullptr;
};
