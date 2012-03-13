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

#ifndef RAOPCALLBACKHANDLER_H
#define RAOPCALLBACKHANDLER_H

#include <QObject>

#include "audiooutput.h"

class RaopCallbackHandler : public QObject
{
    Q_OBJECT
public:
    explicit RaopCallbackHandler(QObject *parent = 0);

private:
    QList<AudioOutput*>  m_outputList;

signals:

public slots:
    void audioInit(void *session, int bits, int channels, int samplerate);
    void audioSetVolume(void *session, float volume);
    void audioProcess(void *session, void *buffer, int buflen);
    void audioFlush(void *session);
    void audioDestroy(void *session);
};

#endif // RAOPCALLBACKHANDLER_H
