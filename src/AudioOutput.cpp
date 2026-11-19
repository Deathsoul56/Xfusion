#include "AudioOutput.h"

#include <SDL3/SDL.h>
#include <cstdio>

AudioOutput::AudioOutput(double sampleRate)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = static_cast<int>(sampleRate);
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!m_stream) {
        std::fprintf(stderr, "SDL audio open failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    m_maxQueuedBytes = static_cast<size_t>(spec.freq) * 4 / 4;
}

AudioOutput::~AudioOutput()
{
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void AudioOutput::push(const int16_t* samples, size_t frames)
{
    if (!m_stream)
        return;
    m_pushedBytes.fetch_add(frames * 4, std::memory_order_relaxed);
    const int queued = SDL_GetAudioStreamQueued(m_stream);
    if (queued == 0 && m_running.load(std::memory_order_relaxed))
        m_underruns.fetch_add(1, std::memory_order_relaxed);
    if (queued < m_minQueued.load(std::memory_order_relaxed))
        m_minQueued.store(queued, std::memory_order_relaxed);
    if (queued > m_maxQueued.load(std::memory_order_relaxed))
        m_maxQueued.store(queued, std::memory_order_relaxed);
    if (static_cast<size_t>(queued) > m_maxQueuedBytes)
        return;
    SDL_PutAudioStreamData(m_stream, samples, static_cast<int>(frames * 4));
}

int AudioOutput::queuedBytes() const
{
    return m_stream ? SDL_GetAudioStreamQueued(m_stream) : 0;
}

AudioOutput::Stats AudioOutput::takeStats()
{
    Stats stats;
    stats.underruns = m_underruns.exchange(0);
    stats.minQueuedBytes = m_minQueued.exchange(INT32_MAX);
    stats.maxQueuedBytes = m_maxQueued.exchange(0);
    if (stats.minQueuedBytes == INT32_MAX)
        stats.minQueuedBytes = 0;
    return stats;
}

void AudioOutput::setPaused(bool paused)
{
    if (!m_stream)
        return;
    m_running = !paused;
    if (paused)
        SDL_PauseAudioStreamDevice(m_stream);
    else
        SDL_ResumeAudioStreamDevice(m_stream);
}
