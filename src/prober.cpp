#include "prober.h"

#include "util.h"

#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
}

Prober::Prober()
{
    m_thread = std::thread([this] { run(); });
}

Prober::~Prober()
{
    m_abort.store(true);
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void Prober::probe(const std::string &url)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(url);
    }
    m_cv.notify_one();
}

std::vector<ProbeResult> Prober::takeResults()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProbeResult> out;
    out.swap(m_results);
    return out;
}

void Prober::run()
{
    while (!m_abort.load()) {
        std::string url;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (m_queue.empty() && !m_abort.load())
                m_cv.wait_for(lock, std::chrono::milliseconds(100));
            if (m_abort.load())
                break;
            url = m_queue.front();
            m_queue.pop_front();
        }

        const std::string path = util::isFileUrl(url) ? util::urlToPath(url) : url;
        AVFormatContext *fmt = nullptr;
        int64_t ms = 0;
        if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) >= 0) {
            if (avformat_find_stream_info(fmt, nullptr) >= 0) {
                if (fmt->duration > 0) {
                    ms = fmt->duration / 1000;
                } else {
                    const int si = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
                    if (si >= 0 && fmt->streams[si]->duration > 0) {
                        ms = av_rescale_q(fmt->streams[si]->duration, fmt->streams[si]->time_base,
                                          AV_TIME_BASE_Q) /
                             1000;
                    }
                }
            }
            avformat_close_input(&fmt);
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_results.push_back({url, ms});
    }
}
