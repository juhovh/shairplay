#include "raopservice.h"

#include <QDebug>

#include <shairplay/raop.h>

#define RSA_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n"\
"MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt\n"\
"wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n"\
"wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf\n"\
"/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n"\
"UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n"\
"BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n"\
"LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5\n"\
"NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm\n"\
"lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n"\
"aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n"\
"a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM\n"\
"oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z\n"\
"oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+\n"\
"k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n"\
"AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA\n"\
"cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf\n"\
"54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n"\
"17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n"\
"1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n"\
"LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n"\
"2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=\n"\
"-----END RSA PRIVATE KEY-----\n"


static void*
audio_init_cb(void *cls, int bits, int channels, int samplerate)
{
    void *session;
    QMetaObject::invokeMethod((QObject*)cls, "audioInit", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, (void*)&session),
                              Q_ARG(int, bits),
                              Q_ARG(int, channels),
                              Q_ARG(int, samplerate));
    return session;
}

static void
audio_process_cb(void *cls, void *session, const void *buffer, int buflen)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioProcess", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(void*, (void*)buffer),
                              Q_ARG(int, buflen));
}

static void
audio_destroy_cb(void *cls, void *session)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioDestroy", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session));
}

static void
audio_flush_cb(void *cls, void *session)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioFlush", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session));
}

static void
audio_set_volume_cb(void *cls, void *session, float volume)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioSetVolume", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(float, volume));
}

static void
audio_set_metadata_cb(void *cls, void *session, const void *buffer, int buflen)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioSetVolume", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(void*, (void*)buffer),
                              Q_ARG(int, buflen));
}

static void
audio_set_coverart_cb(void *cls, void *session, const void *buffer, int buflen)
{
    QMetaObject::invokeMethod((QObject*)cls, "audioSetVolume", Qt::BlockingQueuedConnection,
                              Q_ARG(void*, session),
                              Q_ARG(void*, (void*)buffer),
                              Q_ARG(int, buflen));
}

RaopService::RaopService(QObject *parent) :
    QObject(parent),
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
    raop_destroy(m_raop);
}

bool RaopService::init(int max_clients, RaopCallbacks *callbacks)
{
    raop_callbacks_t raop_cbs;

    m_handler.init(callbacks);
    raop_cbs.cls = &m_handler;
    raop_cbs.audio_init = &audio_init_cb;
    raop_cbs.audio_process = &audio_process_cb;
    raop_cbs.audio_destroy = &audio_destroy_cb;
    raop_cbs.audio_flush = &audio_flush_cb;
    raop_cbs.audio_set_volume = &audio_set_volume_cb;
    raop_cbs.audio_set_metadata = &audio_set_metadata_cb;
    raop_cbs.audio_set_coverart = &audio_set_coverart_cb;

    m_raop = raop_init(max_clients, &raop_cbs, RSA_KEY);
    if (!m_raop) {
        printf("Foobar\n");
        return false;
    }
    return true;
}

bool RaopService::isRunning()
{
    return (raop_is_running(m_raop) != 0);
}

bool RaopService::start(quint16 port, const QByteArray & hwaddr)
{
    int ret;
    m_thread.start();
    ret = raop_start(m_raop, &port, hwaddr.data(), hwaddr.size(), 0);
    if (ret < 0) {
        m_thread.quit();
        m_thread.wait();
        return false;
    }
    return true;
}

void RaopService::stop()
{
    if (m_raop) {
        raop_stop(m_raop);
    }
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}
