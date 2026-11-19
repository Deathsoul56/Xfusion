#include "EmuThread.h"

#include <SDL3/SDL.h>

#include "AudioOutput.h"
#include "LibretroCore.h"

namespace {

// How much audio is kept queued. Lower means less audio/video offset, higher tolerates scheduling jitter.
constexpr double kTargetQueuedFrames = 1.5;
// Longest wait for the audio device to drain before the frame is run anyway (device stalled or unplugged).
constexpr int kMaxWaitFrames = 4;
// Lateness, in frames, beyond which the clock timeline restarts instead of running a burst to catch up.
constexpr int kResyncFrames = 4;

}

EmuThread::EmuThread(LibretroCore& core, AudioOutput* audio, InputPoller pollInput)
    : m_core(core)
    , m_audio(audio && audio->isOpen() ? audio : nullptr)
    , m_pollInput(std::move(pollInput))
{
}

EmuThread::~EmuThread()
{
    stop();
}

void EmuThread::start()
{
    if (m_thread.joinable())
        return;
    m_stop = false;
    m_thread = std::thread([this] { loop(); });
}

void EmuThread::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_stop = true;
    }
    m_pauseChanged.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void EmuThread::setPaused(bool paused)
{
    if (m_audio && paused)
        m_audio->setPaused(true);
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_paused = paused;
    }
    if (m_audio && !paused && m_audioStarted)
        m_audio->setPaused(false);
    m_pauseChanged.notify_all();
}

void EmuThread::sleepUntil(int64_t deadlineNs)
{
    const int64_t remaining = deadlineNs - static_cast<int64_t>(SDL_GetTicksNS());
    if (remaining > 0)
        SDL_DelayPrecise(static_cast<Uint64>(remaining));
}

void EmuThread::waitForAudioDrain(int targetBytes, int64_t frameNs)
{
    const int64_t giveUp = static_cast<int64_t>(SDL_GetTicksNS()) + frameNs * kMaxWaitFrames;
    while (!m_stop && !m_paused && m_audio->queuedBytes() >= targetBytes
           && static_cast<int64_t>(SDL_GetTicksNS()) < giveUp) {
        SDL_DelayNS(500'000);
    }
}

void EmuThread::loop()
{
    const LibretroCore::AvInfo av = m_core.avInfo();
    const int64_t frameNs = static_cast<int64_t>(1e9 / av.fps);
    const int targetBytes = static_cast<int>(av.sampleRate / av.fps * 4 * kTargetQueuedFrames);
    int64_t nextNs = static_cast<int64_t>(SDL_GetTicksNS());

    while (!m_stop) {
        if (m_paused) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseChanged.wait(lock, [this] { return m_stop || !m_paused; });
            nextNs = static_cast<int64_t>(SDL_GetTicksNS());
            continue;
        }

        const uint64_t pushedBefore = m_audio ? m_audio->pushedBytes() : 0;
        {
            std::lock_guard<std::mutex> lock(m_coreMutex);
            if (m_pollInput)
                m_pollInput();
            m_core.run();
        }
        ++m_frames;

        // A core that produced no audio this frame cannot be paced by the audio clock.
        if (m_audio && m_audio->pushedBytes() != pushedBefore) {
            // The device stays paused until the queue is primed, so playback never starts on an empty buffer.
            if (!m_audioStarted && m_audio->queuedBytes() >= targetBytes) {
                m_audioStarted = true;
                if (!m_paused)
                    m_audio->setPaused(false);
            }
            if (m_audioStarted)
                waitForAudioDrain(targetBytes, frameNs);
            nextNs = static_cast<int64_t>(SDL_GetTicksNS());
        } else {
            nextNs += frameNs;
            sleepUntil(nextNs);
            if (static_cast<int64_t>(SDL_GetTicksNS()) - nextNs > frameNs * kResyncFrames)
                nextNs = static_cast<int64_t>(SDL_GetTicksNS());
        }
    }
}
