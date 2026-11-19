#pragma once

#include <cstddef>
#include <cstdint>

struct SDL_AudioStream;

class AudioOutput {
public:
    explicit AudioOutput(double sampleRate);
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool isOpen() const { return m_stream != nullptr; }
    void push(const int16_t* samples, size_t frames);
    void setPaused(bool paused);

private:
    SDL_AudioStream* m_stream = nullptr;
    size_t m_maxQueuedBytes = 0;
};
