#ifndef DNSSDSERVICE_H
#define DNSSDSERVICE_H

#include <QObject>

#include "dnssd.h"

class DnssdService : public QObject
{
    Q_OBJECT
public:
    explicit DnssdService(QObject *parent = 0);
    ~DnssdService();

    bool init();

    void registerRaop(const QString & name, quint16 port, const QByteArray & hwaddr);
    void unregisterRaop();

    void registerAirplay(const QString & name, quint16 port, const QByteArray & hwaddr);
    void unregisterAirplay();

private:
    dnssd_t *  m_dnssd;

signals:

public slots:

};

#endif // DNSSDSERVICE_H
