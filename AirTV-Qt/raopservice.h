#ifndef RAOPSERVICE_H
#define RAOPSERVICE_H

#include <QObject>
#include <QThread>

#include "raopcallbackhandler.h"

#include "dnssd.h"
#include "raop.h"

class RaopService : public QObject
{
    Q_OBJECT
public:
    explicit RaopService(QObject *parent = 0);
    ~RaopService();

    bool init();
    bool start(const QString & name=QString("AirTV"), quint16 port=5000);
    void stop();

private:
    dnssd_t *            m_dnssd;
    raop_t *             m_raop;

    QThread              m_thread;
    RaopCallbackHandler  m_handler;

signals:

public slots:

};

#endif // RAOPSERVICE_H
