#include "raopcallbackhandler.h"

RaopCallbackHandler::RaopCallbackHandler(QObject *parent) :
    QObject(parent)
{
}

void RaopCallbackHandler::init(RaopCallbacks *callbacks)
{
    m_callbacks = callbacks;
}

void RaopCallbackHandler::audioInit(void *session, int bits, int channels, int samplerate)
{
    void **retval = (void**)session;
    if (m_callbacks) {
        *retval = m_callbacks->audioInit(bits, channels, samplerate);
    }
}

void RaopCallbackHandler::audioProcess(void *session, void *buffer, int buflen)
{
    if (m_callbacks) {
        m_callbacks->audioProcess(session, QByteArray((const char *)buffer, buflen));
    }
}

void RaopCallbackHandler::audioDestroy(void *session)
{
    if (m_callbacks) {
        m_callbacks->audioDestroy(session);
    }
}

void RaopCallbackHandler::audioFlush(void *session)
{
    if (m_callbacks) {
        m_callbacks->audioFlush(session);
    }
}

void RaopCallbackHandler::audioSetVolume(void *session, float volume)
{
    if (m_callbacks) {
        m_callbacks->audioSetVolume(session, volume);
    }
}

void RaopCallbackHandler::audioSetMetadata(void *session, void *buffer, int buflen)
{
    if (m_callbacks) {
        m_callbacks->audioSetMetadata(session, QByteArray((const char *)buffer, buflen));
    }
}

void RaopCallbackHandler::audioSetCoverart(void *session, void *buffer, int buflen)
{
    if (m_callbacks) {
        m_callbacks->audioSetCoverart(session, QByteArray((const char *)buffer, buflen));
    }
}

