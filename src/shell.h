#pragma once
#include <QObject>
#include <QStringList>
#include <QUrl>

class QQuickWindow;

class Shell : public QObject
{
    Q_OBJECT
public:
    explicit Shell(QObject *parent = nullptr);
    void setWindow(QQuickWindow *w) { m_window = w; }

    Q_INVOKABLE void startMove();
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void toggleFullscreen();
    Q_INVOKABLE void quit();
    Q_INVOKABLE QUrl demoUrl(int index) const;
    Q_INVOKABLE QStringList audioFilesInFolder(const QUrl &folder) const;
    Q_INVOKABLE QString loadPlaylist() const;
    Q_INVOKABLE void savePlaylist(const QString &json);

signals:
    void aboutToQuit();

private:
    QQuickWindow *m_window = nullptr;
};
