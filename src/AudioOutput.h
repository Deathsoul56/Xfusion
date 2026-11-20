#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <QString>
#include <QStringList>

struct SDL_AudioStream;

class AudioOutput {
public:
    struct Stats {
        unsigned underruns = 0;
        int minQueuedBytes = 0;
        int maxQueuedBytes = 0;
    };

    // `sampleRate` is what the core produces. `deviceName` is an SDL playback device (empty or unknown: the system
    // default) and `playbackRate` the rate the device runs at (0: the core's own rate); SDL converts in between.
    explicit AudioOutput(double sampleRate, const QString& deviceName = {}, int playbackRate = 0);
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool isOpen() const { return m_stream != nullptr; }
    // Linear gain, 0 (silent) to 1. Safe to call while the emulation thread pushes samples.
    void setGain(float gain);
    // Plays the queued audio faster or slower (and higher or lower), so the emulation can run at another speed while
    // it still waits on the audio queue.
    void setSpeedRatio(float ratio);
    // The names of the playback devices SDL can see, listed the first time this is called (at startup).
    static QStringList playbackDevices();
    // The device starts paused; call setPaused(false) once enough audio is queued.
    void push(const int16_t* samples, size_t frames);
    void setPaused(bool paused);
    int queuedBytes() const;
    uint64_t pushedBytes() const { return m_pushedBytes.load(std::memory_order_relaxed); }
    // Returns the counters gathered since the previous call and resets them.
    Stats takeStats();

private:
    SDL_AudioStream* m_stream = nullptr;
    uint32_t m_device = 0;
    size_t m_maxQueuedBytes = 0;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_pushedBytes{0};
    std::atomic<unsigned> m_underruns{0};
    std::atomic<int> m_minQueued{INT32_MAX};
    std::atomic<int> m_maxQueued{0};
};
