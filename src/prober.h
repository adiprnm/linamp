/* ------------------------------------------------------------------
   Linamp — duration prober (background thread, FFmpeg)
   ------------------------------------------------------------------ */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ProbeResult {
    std::string url;
    int64_t durationMs = 0;
};

class Prober {
public:
    Prober();
    ~Prober();

    void probe(const std::string &url);
    /* Returns and clears durations computed since the last call. */
    std::vector<ProbeResult> takeResults();

private:
    void run();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::string> m_queue;
    std::vector<ProbeResult> m_results;
    std::atomic<bool> m_abort{false};
    std::thread m_thread;
};
