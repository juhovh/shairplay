/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

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
