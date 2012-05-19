#ifndef RAOPCALLBACKS_H
#define RAOPCALLBACKS_H

#include <QObject>

class RaopCallbacks : public QObject
{
    Q_OBJECT
public:
    explicit RaopCallbacks(QObject *parent = 0) : QObject(parent) {}

    virtual void * audioInit(int bits, int channels, int samplerate) = 0;
    virtual void audioProcess(void *session, const QByteArray & buffer) = 0;
    virtual void audioDestroy(void *session) = 0;

    virtual void audioFlush(void *session) { Q_UNUSED(session) }
    virtual void audioSetVolume(void *session, float volume) { Q_UNUSED(session) Q_UNUSED(volume) }
    virtual void audioSetMetadata(void *session, const QByteArray & buffer) { Q_UNUSED(session) Q_UNUSED(buffer) }
    virtual void audioSetCoverart(void *session, const QByteArray & buffer) { Q_UNUSED(session) Q_UNUSED(buffer) }

signals:

public slots:

};

#endif // RAOPCALLBACKS_H
