#include "dnssdservice.h"

DnssdService::DnssdService(QObject *parent) :
    QObject(parent)
{
}

bool DnssdService::init()
{
    int error;
    m_dnssd = dnssd_init(&error);
    if (!m_dnssd) {
        return false;
    }
    return true;
}

DnssdService::~DnssdService()
{
    dnssd_destroy(m_dnssd);
}

void DnssdService::registerRaop(const QString & name, quint16 port, const QByteArray & hwaddr)
{
    dnssd_register_raop(m_dnssd, name.toUtf8().data(), port, hwaddr.data(), hwaddr.size(), 0);
}

void DnssdService::unregisterRaop()
{
    dnssd_unregister_raop(m_dnssd);
}

void DnssdService::registerAirplay(const QString &name, quint16 port, const QByteArray &hwaddr)
{
    dnssd_register_airplay(m_dnssd, name.toUtf8().data(), port, hwaddr.data(), hwaddr.size());
}

void DnssdService::unregisterAirplay()
{
    dnssd_unregister_airplay(m_dnssd);
}
