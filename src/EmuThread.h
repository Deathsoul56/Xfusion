#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "AppSettings.h"
#include "RewindBuffer.h"

class AudioOutput;
class LibretroCore;

// How fast and how smoothly the emulation runs.
struct SpeedConfig {
    SpeedMethod method = SpeedMethod::SoundSync;
    double ratio = 1.0; // speed relative to the core's own rate (the "frame rate" setting over the native rate)
    int maxFrameSkip = 0; // Timer with frame skipping: most frames in a row, 0 for no limit
    int fastForwardSkip = 9; // fast-forward runs (skip + 1) times faster, drawing one frame in (skip + 1)
    int fixedFrameSkip = 0; // frames not drawn between two drawn ones, always
    int bufferMs = 64; // sound synchronization: how much audio is kept queued is half of this
};

// Runs the core on its own thread so menus, window moves and painting on the GUI thread can never
// stall emulation. Pacing follows the audio device clock; without audio, or when the user asks for it, it falls
// back to the system clock. Also keeps the rewind history and runs single frames while paused.
class EmuThread {
public:
    using InputPoller = std::function<void()>;

    EmuThread(LibretroCore& core, AudioOutput* audio, InputPoller pollInput);
    ~EmuThread();
    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    // Rewind keeps a snapshot every `interval` frames within `bytes` of memory (0 turns it off). Call before start().
    void setRewind(std::size_t bytes, int interval);
    void start();
    void stop();
    void setPaused(bool paused);
    bool paused() const { return m_paused; }
    // Runs one frame while paused.
    void stepFrame();
    // True while the frame being run is a single step, so the audio callback can treat it differently.
    bool stepping() const { return m_stepping; }

    void setSpeed(const SpeedConfig& speed);
    void setFastForward(bool on) { m_fastForward = on; }
    bool fastForward() const { return m_fastForward; }
    // Plays the game backwards while true, until the history runs out.
    void setRewinding(bool on) { m_rewinding = on; }
    bool rewinding() const { return m_rewinding; }
    bool rewindAvailable() const { return m_rewindEnabled; }

    // Runs fn while the emulation thread is between frames, for operations that touch the core.
    template <class F>
    void withCore(F&& fn)
    {
        std::lock_guard<std::mutex> lock(m_coreMutex);
        fn();
    }

    // Frames emulated since the previous call; the two counters are independent so --stats and the frame rate
    // overlay do not steal from each other.
    int takeFrameCount() { return m_frames.exchange(0); }
    int takeFpsFrames() { return m_fpsFrames.exchange(0); }

private:
    void loop();
    void waitForAudioDrain(int targetBytes, int64_t frameNs);
    void sleepUntil(int64_t deadlineNs);
    SpeedConfig speed() const;

    LibretroCore& m_core;
    AudioOutput* m_audio;
    InputPoller m_pollInput;

    std::thread m_thread;
    std::mutex m_coreMutex;
    std::mutex m_pauseMutex;
    mutable std::mutex m_speedMutex;
    std::condition_variable m_pauseChanged;
    SpeedConfig m_speed;
    RewindBuffer m_rewind;
    int m_rewindInterval = 5;
    bool m_rewindEnabled = false;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_step{false};
    std::atomic<bool> m_stepping{false};
    std::atomic<bool> m_fastForward{false};
    std::atomic<bool> m_rewinding{false};
    std::atomic<bool> m_audioStarted{false};
    std::atomic<int> m_frames{0};
    std::atomic<int> m_fpsFrames{0};
};
