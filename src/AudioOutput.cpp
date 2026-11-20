#include "AudioOutput.h"

#include <SDL3/SDL.h>
#include <cstdio>

namespace {

SDL_AudioDeviceID findDevice(const QString& name)
{
    if (name.isEmpty())
        return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    SDL_AudioDeviceID found = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    for (int i = 0; ids && i < count; ++i) {
        const char* deviceName = SDL_GetAudioDeviceName(ids[i]);
        if (deviceName && name == QString::fromUtf8(deviceName)) {
            found = ids[i];
            break;
        }
    }
    SDL_free(ids);
    return found;
}

}

// Enumerated once, before any game has opened the audio device: asking SDL again while it plays can stall the caller.
QStringList AudioOutput::playbackDevices()
{
    static QStringList names;
    static bool done = false;
    if (done)
        return names;
    done = true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return names;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    for (int i = 0; ids && i < count; ++i)
        if (const char* name = SDL_GetAudioDeviceName(ids[i]))
            names << QString::fromUtf8(name);
    SDL_free(ids);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return names;
}

AudioOutput::AudioOutput(double sampleRate, const QString& deviceName, int playbackRate)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec source{};
    source.format = SDL_AUDIO_S16;
    source.channels = 2;
    source.freq = static_cast<int>(sampleRate);
    SDL_AudioSpec device = source;
    if (playbackRate > 0)
        device.freq = playbackRate;

    // The device opens running; it is paused until the emulation thread has queued enough audio.
    m_device = SDL_OpenAudioDevice(findDevice(deviceName), &device);
    if (!m_device && !deviceName.isEmpty())
        m_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device);
    if (!m_device) {
        std::fprintf(stderr, "SDL audio open failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    SDL_PauseAudioDevice(m_device);
    m_stream = SDL_CreateAudioStream(&source, &device);
    if (!m_stream || !SDL_BindAudioStream(m_device, m_stream)) {
        std::fprintf(stderr, "SDL audio stream failed: %s\n", SDL_GetError());
        if (m_stream)
            SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    m_maxQueuedBytes = static_cast<size_t>(source.freq) * 4 / 4;
}

AudioOutput::~AudioOutput()
{
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        SDL_CloseAudioDevice(m_device);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void AudioOutput::setGain(float gain)
{
    if (m_stream)
        SDL_SetAudioStreamGain(m_stream, gain);
}

void AudioOutput::setSpeedRatio(float ratio)
{
    if (m_stream)
        SDL_SetAudioStreamFrequencyRatio(m_stream, ratio);
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
        SDL_PauseAudioDevice(m_device);
    else
        SDL_ResumeAudioDevice(m_device);
}