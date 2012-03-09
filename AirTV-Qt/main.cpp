#include <QtGui>
#include <QtSingleApplication>

#include "mainapplication.h"
#include "videowidget.h"
#include "raopservice.h"

int main(int argc, char *argv[])
{
    QtSingleApplication a(argc, argv);
    if (a.isRunning()) {
        return 0;
    }
    a.setApplicationName("AirTV");

    if (!QSystemTrayIcon::isSystemTrayAvailable())  {
        QMessageBox::critical(0, QObject::tr("Systray"),
                              QObject::tr("I couldn't detect any system tray "
                                          "on this system."));
        return 1;
    }
    QApplication::setQuitOnLastWindowClosed(false);

    MainApplication m;
    QObject::connect(&m, SIGNAL(quitRequested()), &a, SLOT(quit()));
    QObject::connect(&a, SIGNAL(aboutToQuit()), &m, SLOT(aboutToQuit()));
    m.start();

    return a.exec();
}
