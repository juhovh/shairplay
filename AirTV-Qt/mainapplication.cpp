#include "mainapplication.h"

#include <QDebug>

MainApplication::MainApplication(QObject *parent) :
    QObject(parent)
{
    raopService = new RaopService(0);
    dnssdService = new DnssdService(0);
    trayIconMenu = new QMenu(0);

    // Initialize the service
    raopService->init(10, &m_callbacks);
    dnssdService->init();

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
    char chwaddr[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB };
    QByteArray hwaddr(chwaddr, sizeof(chwaddr));

    raopService->start(5000, hwaddr);
    dnssdService->registerRaop("Shairplay", 5000, hwaddr);
    trayIcon->show();
}

void MainApplication::stop()
{
    dnssdService->unregisterRaop();
    raopService->stop();
    trayIcon->hide();
}

void MainApplication::aboutToQuit()
{
    this->stop();
}
