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

#include "raopcallbackhandler.h"

RaopCallbackHandler::RaopCallbackHandler(QObject *parent) :
    QObject(parent)
{
}

void RaopCallbackHandler::audioInit(void *session, int bits, int channels, int samplerate)
{
    void **retval = (void**)session;

    AudioOutput *audioOutput = new AudioOutput(0);
    audioOutput->init(bits, channels, samplerate);
    audioOutput->start();
    *retval = audioOutput;

    m_outputList.append(audioOutput);
}

void RaopCallbackHandler::audioSetVolume(void *session, float volume)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->setVolume(volume);
}

void RaopCallbackHandler::audioProcess(void *session, void *buffer, int buflen)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->output((const char *)buffer, buflen);
}

void RaopCallbackHandler::audioFlush(void *session)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->flush();
}

void RaopCallbackHandler::audioDestroy(void *session)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    m_outputList.removeAll(audioOutput);

    audioOutput->stop();
    delete audioOutput;
}
