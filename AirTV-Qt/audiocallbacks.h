#ifndef AUDIOCALLBACKS_H
#define AUDIOCALLBACKS_H

#include "raopcallbacks.h"

#include "audiooutput.h"

class AudioCallbacks : public RaopCallbacks
{
    Q_OBJECT
public:
    explicit AudioCallbacks(QObject *parent = 0);

    virtual void * audioInit(int bits, int channels, int samplerate);
    virtual void audioSetVolume(void *session, float volume);
    virtual void audioProcess(void *session, const QByteArray &buffer);
    virtual void audioFlush(void *session);
    virtual void audioDestroy(void *session);


private:
    QList<AudioOutput*>  m_outputList;

signals:

public slots:

};

#endif // AUDIOCALLBACKS_H
