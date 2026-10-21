#include "prober.h"

#include <QUrl>
#include <QFileInfo>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
}

class ProberWorker : public QThread
{
    Q_OBJECT
public:
    ~ProberWorker() override { stop(); wait(); }
    void enqueue(const QString &url)
    {
        QMutexLocker lock(&m_mutex);
        m_queue.enqueue(url);
        m_wait.wakeOne();
    }
    void stop()
    {
        m_abort.store(true);
        QMutexLocker lock(&m_mutex);
        m_wait.wakeAll();
    }

signals:
    void probed(const QString &url, qint64 durationMs);

protected:
    void run() override
    {
        while (!m_abort.load()) {
            QString url;
            {
                QMutexLocker lock(&m_mutex);
                while (m_queue.isEmpty() && !m_abort.load())
                    m_wait.wait(&m_mutex, 100);
                if (m_abort.load())
                    break;
                url = m_queue.dequeue();
            }

            const QString local = url.startsWith("file:") ? QUrl(url).toLocalFile() : url;
            const QByteArray path = (local.isEmpty() ? url : local).toUtf8();
            AVFormatContext *fmt = nullptr;
            qint64 ms = 0;
            if (avformat_open_input(&fmt, path.constData(), nullptr, nullptr) >= 0) {
                if (avformat_find_stream_info(fmt, nullptr) >= 0) {
                    if (fmt->duration > 0) {
                        ms = fmt->duration / 1000;
                    } else {
                        const int si = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
                        if (si >= 0 && fmt->streams[si]->duration > 0) {
                            ms = av_rescale_q(fmt->streams[si]->duration,
                                              fmt->streams[si]->time_base, AV_TIME_BASE_Q) / 1000;
                        }
                    }
                }
                avformat_close_input(&fmt);
            }
            emit probed(url, ms);
        }
    }

private:
    QMutex m_mutex;
    QWaitCondition m_wait;
    QQueue<QString> m_queue;
    std::atomic<bool> m_abort{false};
};

Prober::Prober(QObject *parent) : QObject(parent)
{
    m_worker = new ProberWorker;
    connect(m_worker, &ProberWorker::probed, this, &Prober::probed, Qt::QueuedConnection);
    m_worker->start();
}

Prober::~Prober()
{
    if (m_worker) {
        m_worker->stop();
        m_worker->wait(3000);
        delete m_worker;
        m_worker = nullptr;
    }
}

void Prober::probe(const QString &url)
{
    if (m_worker)
        m_worker->enqueue(url);
}

#include "prober.moc"
