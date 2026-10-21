#pragma once
#include <QObject>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QThread>
#include <atomic>

class ProberWorker;

class Prober : public QObject
{
    Q_OBJECT
public:
    explicit Prober(QObject *parent = nullptr);
    ~Prober() override;

    Q_INVOKABLE void probe(const QString &url);

signals:
    void probed(const QString &url, qint64 durationMs);

private:
    ProberWorker *m_worker = nullptr;
};
