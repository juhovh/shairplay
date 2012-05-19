#ifndef RAOPCALLBACKHANDLER_H
#define RAOPCALLBACKHANDLER_H

#include <QObject>

#include "raopcallbacks.h"

class RaopCallbackHandler : public QObject
{
    Q_OBJECT
public:
    explicit RaopCallbackHandler(QObject *parent = 0);
    void init(RaopCallbacks *callbacks);

private:
    RaopCallbacks * m_callbacks;

signals:

public slots:
    void audioInit(void *session, int bits, int channels, int samplerate);
    void audioProcess(void *session, void *buffer, int buflen);
    void audioDestroy(void *session);
    void audioFlush(void *session);
    void audioSetVolume(void *session, float volume);
    void audioSetMetadata(void *session, void *buffer, int buflen);
    void audioSetCoverart(void *session, void *buffer, int buflen);
};

#endif // RAOPCALLBACKHANDLER_H
