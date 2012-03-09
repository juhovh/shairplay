#include "mainapplication.h"

#include <QDebug>

MainApplication::MainApplication(QObject *parent) :
    QObject(parent)
{
    raopService = new RaopService(0);
    trayIconMenu = new QMenu(0);

    // Initialize the service
    raopService->init();

    quitAction = new QAction(tr("&Quit"), trayIconMenu);
    connect(quitAction, SIGNAL(triggered()), this, SIGNAL(quitRequested()));
    trayIconMenu->addAction(quitAction);

    // Construct the actual system tray icon
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon(":icons/airtv.svg"));
}

MainApplication::~MainApplication()
{
    trayIcon->setContextMenu(0);
    delete trayIconMenu;
    delete raopService;
}

void MainApplication::start()
{
    raopService->start();
    trayIcon->show();
}

void MainApplication::stop()
{
    raopService->stop();
    trayIcon->hide();
}

void MainApplication::aboutToQuit()
{
    this->stop();
}
