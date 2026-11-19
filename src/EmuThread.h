#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

class AudioOutput;
class LibretroCore;

// Runs the core on its own thread so menus, window moves and painting on the GUI thread can never
// stall emulation. Pacing follows the audio device clock; without audio it falls back to the system clock.
class EmuThread {
public:
    using InputPoller = std::function<void()>;

    EmuThread(LibretroCore& core, AudioOutput* audio, InputPoller pollInput);
    ~EmuThread();
    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    void start();
    void stop();
    void setPaused(bool paused);

    // Runs fn while the emulation thread is between frames, for operations that touch the core.
    template <class F>
    void withCore(F&& fn)
    {
        std::lock_guard<std::mutex> lock(m_coreMutex);
        fn();
    }

    int takeFrameCount() { return m_frames.exchange(0); }

private:
    void loop();
    void waitForAudioDrain(int targetBytes, int64_t frameNs);
    void sleepUntil(int64_t deadlineNs);

    LibretroCore& m_core;
    AudioOutput* m_audio;
    InputPoller m_pollInput;

    std::thread m_thread;
    std::mutex m_coreMutex;
    std::mutex m_pauseMutex;
    std::condition_variable m_pauseChanged;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_audioStarted{false};
    std::atomic<int> m_frames{0};
};
