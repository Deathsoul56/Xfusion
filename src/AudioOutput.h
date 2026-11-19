#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

struct SDL_AudioStream;

class AudioOutput {
public:
    struct Stats {
        unsigned underruns = 0;
        int minQueuedBytes = 0;
        int maxQueuedBytes = 0;
    };

    explicit AudioOutput(double sampleRate);
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool isOpen() const { return m_stream != nullptr; }
    // The device starts paused; call setPaused(false) once enough audio is queued.
    void push(const int16_t* samples, size_t frames);
    void setPaused(bool paused);
    int queuedBytes() const;
    uint64_t pushedBytes() const { return m_pushedBytes.load(std::memory_order_relaxed); }
    // Returns the counters gathered since the previous call and resets them.
    Stats takeStats();

private:
    SDL_AudioStream* m_stream = nullptr;
    size_t m_maxQueuedBytes = 0;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_pushedBytes{0};
    std::atomic<unsigned> m_underruns{0};
    std::atomic<int> m_minQueued{INT32_MAX};
    std::atomic<int> m_maxQueued{0};
};
