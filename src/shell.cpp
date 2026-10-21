#include "shell.h"

#include <QQuickWindow>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>

Shell::Shell(QObject *parent) : QObject(parent)
{
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Shell::aboutToQuit);
}

void Shell::startMove()
{
    if (m_window)
        m_window->startSystemMove();
}

void Shell::minimize()
{
    if (m_window)
        m_window->showMinimized();
}

void Shell::toggleFullscreen()
{
    if (!m_window)
        return;
    if (m_window->visibility() == QWindow::FullScreen)
        m_window->showNormal();
    else
        m_window->showFullScreen();
}

void Shell::quit()
{
    QCoreApplication::quit();
}

QUrl Shell::demoUrl(int index) const
{
    const QString name = QStringLiteral("demo%1.mp3").arg(index == 2 ? 2 : 1);

    // Demo tracks are embedded in the binary (qrc) so releases are self-contained.
    // FFmpeg cannot open qrc paths, so extract them to the cache on first use.
    QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache.isEmpty())
        cache = QDir::tempPath() + QStringLiteral("/linamp");

    const QString dir = cache + QStringLiteral("/demos");
    if (!QDir().mkpath(dir)) {
        qWarning() << "Cannot create demo cache directory:" << dir;
        return QUrl();
    }

    const QString dest = dir + QLatin1Char('/') + name;
    if (!QFile::exists(dest)) {
        const QString src = QStringLiteral(":/assets/") + name;
        if (!QFile::copy(src, dest)) {
            qWarning() << "Cannot extract demo track" << src << "to" << dest;
            return QUrl();
        }
    }
    return QUrl::fromLocalFile(dest);
}

QStringList Shell::audioFilesInFolder(const QUrl &folder) const
{
    QStringList out;
    const QString dir = folder.toLocalFile();
    if (dir.isEmpty())
        return out;
    const QStringList filters = {"*.mp3", "*.wav", "*.ogg", "*.oga", "*.flac",
                                 "*.m4a", "*.aac", "*.opus", "*.wma", "*.aiff", "*.aif"};
    QDirIterator it(dir, filters, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext())
        out.append(QUrl::fromLocalFile(it.next()).toString());
    out.sort(Qt::CaseInsensitive);
    return out;
}

QString Shell::loadPlaylist() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("playlist"));
    return settings.value(QStringLiteral("json")).toString();
}

void Shell::savePlaylist(const QString &json)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("playlist"));
    settings.setValue(QStringLiteral("json"), json);
    settings.endGroup();
    settings.sync();
}
