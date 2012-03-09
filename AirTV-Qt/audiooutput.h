#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QIODevice>
#include <QByteArray>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QIODevice>

class AudioOutput : public QIODevice
{
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = 0);
    bool init(int bits, int channels, int samplerate);
    bool setDevice(QAudioDeviceInfo deviceInfo);

    void start();
    void setVolume(float volume);
    void output(const char *data, int datalen);
    void flush();
    void stop();

    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
    qint64 bytesAvailable() const;
    bool isSequential() const;

private:
    void reinit();

private:
    bool             m_initialized;
    QByteArray       m_buffer;
    QAudioFormat     m_format;
    QAudioDeviceInfo m_deviceInfo;
    QAudioOutput*    m_output;
    float            m_volume;

signals:

public slots:

private slots:
    void notified();
    void stateChanged(QAudio::State state);
};

#endif // AUDIOOUTPUT_H
