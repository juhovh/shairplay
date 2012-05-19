#include "audiocallbacks.h"

AudioCallbacks::AudioCallbacks(QObject *parent) :
    RaopCallbacks(parent)
{
}

void * AudioCallbacks::audioInit(int bits, int channels, int samplerate)
{
    AudioOutput *audioOutput = new AudioOutput(0);
    audioOutput->init(bits, channels, samplerate);
    audioOutput->start();
    m_outputList.append(audioOutput);
    return audioOutput;
}

void AudioCallbacks::audioProcess(void *session, const QByteArray & buffer)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->output(buffer);
}

void AudioCallbacks::audioDestroy(void *session)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    m_outputList.removeAll(audioOutput);

    audioOutput->stop();
    delete audioOutput;
}


void AudioCallbacks::audioFlush(void *session)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->flush();
}

void AudioCallbacks::audioSetVolume(void *session, float volume)
{
    AudioOutput *audioOutput = (AudioOutput*)session;
    audioOutput->setVolume(volume);
}

