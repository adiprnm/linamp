#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QIcon>
#include <QUrl>
#include <QDebug>

#include "audioengine.h"
#include "shell.h"
#include "prober.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickWindow::setDefaultAlphaBuffer(true);
    app.setApplicationName(QStringLiteral("Linamp"));
    app.setApplicationDisplayName(QStringLiteral("Linamp"));
    app.setOrganizationName(QStringLiteral("linamp"));
    app.setOrganizationDomain(QStringLiteral("linamp.local"));
    app.setDesktopFileName(QStringLiteral("linamp"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icon.png")));

    AudioEngine engine;
    Prober prober;
    Shell shell;

    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("engine"), &engine);
    qml.rootContext()->setContextProperty(QStringLiteral("prober"), &prober);
    qml.rootContext()->setContextProperty(QStringLiteral("shell"), &shell);

    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

#ifdef LINAMP_EMBEDDED_QML
    qml.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
#else
    qml.load(QUrl::fromLocalFile(QStringLiteral(APP_QML_DIR "/Main.qml")));
#endif
    if (qml.rootObjects().isEmpty())
        return 1;

    QQuickWindow *win = qobject_cast<QQuickWindow *>(qml.rootObjects().constFirst());
    if (!win)
        return 1;

    shell.setWindow(win);
    win->setMinimumSize(QSize(580, 400));
    return app.exec();
}
