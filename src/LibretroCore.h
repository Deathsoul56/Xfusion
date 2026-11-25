#pragma once

#include <QByteArray>
#include <QImage>
#include <QLibrary>
#include <QString>
#include <array>
#include <atomic>
#include <QTemporaryDir>
#include <cstdint>
#include <memory>
#include <vector>
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
    // Changes to another core: the current one is unloaded (with its game) and `corePath`, described by `profile`,
    // loaded. Does nothing when that core is already the one loaded.
    bool useCore(const CoreProfile& profile, const QString& corePath, QString* error);
    bool loadGame(const QString& gamePath, QString* error);
    void unloadGame();
    // Multi-disc games (libretro disk control). The disc the game was loaded from is disc 0; `addDisc` puts another
    // one behind it, before the game runs. `swapDisc` takes the disc out, puts the chosen one in and closes the tray,
    // and has to run where the core runs. False when the core has no disk control or refuses.
    bool addDisc(const QString& path);
    int discCount() const;
    int discIndex() const;
    bool swapDisc(int index);
    void run();
    void reset();

    bool isLoaded() const { return m_loaded; }
    bool hasGame() const { return m_hasGame; }
    const AvInfo& avInfo() const { return m_av; }
    const QString& name() const { return m_name; }
    // What the core says it is, for the user agent the server sees: "Genesis Plus GX" and "v1.7.4 4fd32ef".
    const QString& libraryName() const { return m_libraryName; }
    const QString& libraryVersion() const { return m_libraryVersion; }
    // The memory the core exposes to the frontend (RetroAchievements reads it): the map the core announced with
    // SET_MEMORY_MAPS, or null when it did not, and a region of retro_get_memory_data (RETRO_MEMORY_*).
    const retro_memory_map* memoryMap() const { return m_hasMemoryMap ? &m_memoryMap : nullptr; }
    bool memoryRegion(unsigned id, uint8_t** data, std::size_t* size) const;

    void setDirectories(const QString& systemDir, const QString& saveDir);
    void setVideoCallback(VideoCallback cb) { m_video = std::move(cb); }
    void setAudioCallback(AudioCallback cb) { m_audio = std::move(cb); }
    static constexpr int kPlayers = 2;
    void setButton(int player, PadButton button, bool pressed);
    // Selects the pad the core emulates. Call from the emulation thread (or with it stopped): the core
    // reads the new value at the start of its next frame.
    // `genesisFamily` is false for the 8-bit systems, whose pad needs no choosing.
    void setPads(PadType type, bool secondPadConnected, bool genesisFamily = true);
    // The options the user changed for this core (key to value). Only those the profile lists, with a value it lists,
    // are taken; every other option the user may change goes back to the core's default. Call before loading a game.
    void setUserOptions(const std::map<std::string, std::string>& values);
    // While set, frames the core produces are not delivered (frame skipping). The core is told it may skip
    // rendering too.
    void setSkipVideo(bool skip) { m_skipVideo.store(skip, std::memory_order_relaxed); }
    const CoreProfile& profile() const { return *m_profile; }
    const QString& corePath() const { return m_corePath; }

    // Battery RAM, as the core exposes it: empty while the core has none (PicoDrive reports it only once it is
    // not all zero). Call from the emulation thread, or with it stopped.
    QByteArray saveRam() const;
    // Copies `data` into the battery RAM. `warning` is set when the file is longer than the core's RAM (nothing is
    // lost then only if the caller refuses to overwrite it); a shorter file is accepted.
    bool setSaveRam(const QByteArray& data, QString* warning);

    // Save states. The state is whatever the core serializes; it is only meaningful for the same core build and
    // game. Call from the emulation thread, or with it stopped.
    bool supportsStates() const;
    bool serialize(QByteArray* state, QString* error);
    bool unserialize(const QByteArray& state, QString* error);

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
    // The path the core is given: the game itself, or what a zip held when the core cannot open zips.
    QString contentPath(const QString& gamePath, QString* error);
    bool reloadCore(QString* error);

    const CoreProfile* m_profile;
    QString m_corePath;
    unsigned m_portDevices[2] = {0, 0};
    bool m_choosePortDevices = false;
    void applyPortDevices();
    std::map<std::string, std::string> m_options;
    bool m_optionsChanged = false;
    QLibrary m_lib;
    bool m_loaded = false;
    bool m_hasGame = false;
    bool m_usedOnce = false;
    QString m_name;
    QString m_libraryName;
    QString m_libraryVersion;
    std::vector<retro_memory_descriptor> m_memoryDescriptors; // copied: the core may reuse the array it announced
    retro_memory_map m_memoryMap{};
    bool m_hasMemoryMap = false;
    AvInfo m_av;
    retro_pixel_format m_format = RETRO_PIXEL_FORMAT_0RGB1555;
    QByteArray m_systemDir;
    QByteArray m_saveDir;
    QByteArray m_gamePath;
    std::vector<QByteArray> m_extraDiscs; // the paths handed to the core, which keeps only the pointers it is given
    retro_disk_control_callback m_disk{};
    bool m_hasDiskControl = false;
    std::unique_ptr<QTemporaryDir> m_unpacked; // the game extracted from a zip, while it is loaded
    std::array<std::array<std::atomic<bool>, 16>, kPlayers> m_buttons{};
    std::atomic<bool> m_skipVideo{false};
    VideoCallback m_video;
    AudioCallback m_audio;

    void (*m_setEnvironment)(retro_environment_t) = nullptr;
    void (*m_setVideoRefresh)(retro_video_refresh_t) = nullptr;
    void (*m_setAudioSample)(retro_audio_sample_t) = nullptr;
    void (*m_setAudioSampleBatch)(retro_audio_sample_batch_t) = nullptr;
    void (*m_setInputPoll)(retro_input_poll_t) = nullptr;
    void (*m_setInputState)(retro_input_state_t) = nullptr;
    void (*m_setControllerPortDevice)(unsigned, unsigned) = nullptr; // optional
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
    // Optional in practice: a core without them simply has no save states.
    size_t (*m_serializeSize)() = nullptr;
    bool (*m_serialize)(void*, size_t) = nullptr;
    bool (*m_unserialize)(const void*, size_t) = nullptr;
};
