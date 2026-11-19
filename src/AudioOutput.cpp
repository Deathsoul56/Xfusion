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
    SDL_ResumeAudioStreamDevice(m_stream);
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
    if (static_cast<size_t>(SDL_GetAudioStreamQueued(m_stream)) > m_maxQueuedBytes)
        return;
    SDL_PutAudioStreamData(m_stream, samples, static_cast<int>(frames * 4));
}

void AudioOutput::setPaused(bool paused)
{
    if (!m_stream)
        return;
    if (paused)
        SDL_PauseAudioStreamDevice(m_stream);
    else
        SDL_ResumeAudioStreamDevice(m_stream);
}
