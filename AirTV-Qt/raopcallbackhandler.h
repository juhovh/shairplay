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
