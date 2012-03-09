#ifndef MAINAPPLICATION_H
#define MAINAPPLICATION_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

#include "raopservice.h"

class MainApplication : public QObject
{
    Q_OBJECT
public:
    explicit MainApplication(QObject *parent = 0);
    ~MainApplication();

    void start();
    void stop();

private:
    RaopService *raopService;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QAction *quitAction;

signals:
    void quitRequested();

public slots:
    void aboutToQuit();

};

#endif // MAINAPPLICATION_H
