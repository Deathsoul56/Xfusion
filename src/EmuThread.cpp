#include "EmuThread.h"

#include <SDL3/SDL.h>
#include <algorithm>

#include "AudioOutput.h"
#include "LibretroCore.h"

namespace {

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

void EmuThread::setRewind(std::size_t bytes, int interval)
{
    m_rewind.reset(bytes);
    m_rewindInterval = std::max(1, interval);
    m_rewindEnabled = bytes > 0 && m_core.supportsStates();
}

void EmuThread::setSpeed(const SpeedConfig& speed)
{
    std::lock_guard<std::mutex> lock(m_speedMutex);
    m_speed = speed;
}

SpeedConfig EmuThread::speed() const
{
    std::lock_guard<std::mutex> lock(m_speedMutex);
    return m_speed;
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

void EmuThread::stepFrame()
{
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_step = true;
    }
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
    const int64_t baseFrameNs = static_cast<int64_t>(1e9 / av.fps);
    int64_t nextNs = static_cast<int64_t>(SDL_GetTicksNS());
    uint64_t frameIndex = 0;
    int consecutiveSkips = 0;
    bool behind = false;
    float audioRatio = 1.0f;

    while (!m_stop) {
        bool stepping = false;
        if (m_paused) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseChanged.wait(lock, [this] { return m_stop || !m_paused || m_step; });
            if (m_stop)
                break;
            if (m_paused) {
                m_step = false;
                stepping = true;
            }
            nextNs = static_cast<int64_t>(SDL_GetTicksNS());
        }

        const SpeedConfig cfg = speed();
        const bool fastForward = m_fastForward && !stepping;
        const bool rewinding = m_rewinding && m_rewindEnabled;

        bool skipVideo = false;
        if (fastForward)
            skipVideo = frameIndex % static_cast<uint64_t>(cfg.fastForwardSkip + 1) != 0;
        else if (cfg.fixedFrameSkip > 0)
            skipVideo = frameIndex % static_cast<uint64_t>(cfg.fixedFrameSkip + 1) != 0;
        else if (cfg.method == SpeedMethod::TimerFrameskip && behind)
            skipVideo = cfg.maxFrameSkip == 0 || consecutiveSkips < cfg.maxFrameSkip;
        consecutiveSkips = skipVideo ? consecutiveSkips + 1 : 0;
        // A single step exists to be seen.
        m_core.setSkipVideo(skipVideo && !stepping);

        const uint64_t pushedBefore = m_audio ? m_audio->pushedBytes() : 0;
        m_stepping = stepping;
        {
            std::lock_guard<std::mutex> lock(m_coreMutex);
            if (m_pollInput)
                m_pollInput();
            if (rewinding) {
                // Each displayed frame goes back one snapshot, which is `interval` frames of history.
                QByteArray state;
                QString error;
                if (m_rewind.pop(&state))
                    m_core.unserialize(state, &error);
                else
                    m_rewinding = false;
            } else if (m_rewindEnabled && frameIndex % static_cast<uint64_t>(m_rewindInterval) == 0) {
                QByteArray state;
                QString error;
                if (m_core.serialize(&state, &error))
                    m_rewind.push(state);
            }
            m_core.run();
        }
        ++frameIndex;
        ++m_frames;
        ++m_fpsFrames;
        if (stepping)
            continue;

        const double ratio = rewinding ? 1.0 : cfg.ratio;
        // Fast-forward plays the sound faster too (higher pitched) unless the frontend drops it.
        const float wantedAudio = fastForward ? static_cast<float>(cfg.fastForwardSkip + 1) : static_cast<float>(ratio);
        if (m_audio && audioRatio != wantedAudio) {
            audioRatio = wantedAudio;
            m_audio->setSpeedRatio(audioRatio);
        }

        // A core that produced no audio this frame (or whose audio is being dropped, as in fast-forward or rewind)
        // cannot be paced by the audio clock.
        const bool audioPaced = cfg.method == SpeedMethod::SoundSync && !fastForward && !rewinding && m_audio
            && m_audio->pushedBytes() != pushedBefore;
        if (audioPaced) {
            const int targetBytes = std::max(static_cast<int>(av.sampleRate / av.fps * 4),
                                             static_cast<int>(av.sampleRate * 4 * (cfg.bufferMs / 2.0) / 1000.0));
            // The device stays paused until the queue is primed, so playback never starts on an empty buffer.
            if (!m_audioStarted && m_audio->queuedBytes() >= targetBytes) {
                m_audioStarted = true;
                if (!m_paused)
                    m_audio->setPaused(false);
            }
            if (m_audioStarted)
                waitForAudioDrain(targetBytes, baseFrameNs);
            nextNs = static_cast<int64_t>(SDL_GetTicksNS());
            behind = false;
        } else if (cfg.method == SpeedMethod::Unlimited && !rewinding) {
            nextNs = static_cast<int64_t>(SDL_GetTicksNS());
            behind = false;
        } else {
            const double speedUp = fastForward ? cfg.fastForwardSkip + 1 : std::max(0.05, ratio);
            const int64_t frameNs = static_cast<int64_t>(baseFrameNs / speedUp);
            nextNs += frameNs;
            sleepUntil(nextNs);
            const int64_t late = static_cast<int64_t>(SDL_GetTicksNS()) - nextNs;
            behind = late >= frameNs;
            if (late > frameNs * kResyncFrames)
                nextNs = static_cast<int64_t>(SDL_GetTicksNS());
        }
    }
}
