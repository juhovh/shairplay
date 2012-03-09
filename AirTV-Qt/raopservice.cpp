#include "raopservice.h"

#include <QDebug>
#include <QFile>

static void
audio_init(void *cls, void **session, int bits, int channels, int samplerate)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioInit", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, (void*)session),
                              Q_ARG(int, bits),
                              Q_ARG(int, channels),
                              Q_ARG(int, samplerate));
}

static void
audio_set_volume(void *cls, void *session, float volume)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioSetVolume", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(float, volume));
}

static void
audio_process(void *cls, void *session, const void *buffer, int buflen)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioProcess", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(void*, (void*)buffer),
                              Q_ARG(int, buflen));
}

static void
audio_flush(void *cls, void *session)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioFlush", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session));
}

static void
audio_destroy(void *cls, void *session)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioDestroy", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session));
}

RaopService::RaopService(QObject *parent) :
    QObject(parent),
    m_dnssd(0),
    m_raop(0)
{
    /* This whole hack is required because QAudioOutput
     * needs to be created in a QThread, threads created
     * outside Qt are not allowed (they have no eventloop) */
    m_handler.moveToThread(&m_thread);
}

RaopService::~RaopService()
{
    this->stop();

    dnssd_destroy(m_dnssd);
    raop_destroy(m_raop);
}

bool RaopService::init()
{
    const char hwaddr[] = { 0x48, 0x5d, 0x60, 0x7c, 0xee, 0x22 };
    raop_callbacks_t raop_cbs;
    int error;

    raop_cbs.cls = &m_handler;
    raop_cbs.audio_init = audio_init;
    raop_cbs.audio_set_volume = audio_set_volume;
    raop_cbs.audio_process = audio_process;
    raop_cbs.audio_flush = audio_flush;
    raop_cbs.audio_destroy = audio_destroy;

    QFile file("airport.key");
    if (!file.exists()) {
        // This is used when running from Qt Creator on Mac
        file.setFileName("../../../../airport.key");
    }
    if (!file.exists()) {
        // This is used when running from Qt Creator on Windows
        file.setFileName("../airport.key");
    }
    if (!file.exists()) {
        return false;
    }
    file.open(QIODevice::ReadOnly);
    QByteArray array = file.read(file.size());
    array.append('\0');

    m_raop = raop_init(&raop_cbs, array.data(), hwaddr, sizeof(hwaddr));
    if (!m_raop) {
        return false;
    }

    m_dnssd = dnssd_init(hwaddr, sizeof(hwaddr), &error);
    if (!m_dnssd) {
        raop_destroy(m_raop);
        m_raop = NULL;
        return false;
    }

    return true;
}

bool RaopService::start(const QString & name, quint16 port)
{
    if (!m_raop || !m_dnssd || m_thread.isRunning()) {
        return false;
    }

    m_thread.start();
    if (raop_start(m_raop, &port) < 0) {
        m_thread.quit();
        m_thread.wait();
        return false;
    }
    if (dnssd_register_raop(m_dnssd, name.toUtf8(), port) < 0) {
        raop_stop(m_raop);
        m_thread.quit();
        m_thread.wait();
        return false;
    }

    return true;
}

void RaopService::stop()
{
    if (m_dnssd) {
        dnssd_unregister_raop(m_dnssd);
    }
    if (m_raop) {
        raop_stop(m_raop);
    }
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

