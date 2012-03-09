#ifndef SOCKETS_H
#define SOCKETS_H

#if defined(WIN32)
typedef int socklen_t;

#ifndef SHUT_RD
#  define SHUT_RD SD_RECEIVE
#endif
#ifndef SHUT_WR
#  define SHUT_WR SD_SEND
#endif
#ifndef SHUT_RDWR
#  define SHUT_RDWR SD_BOTH
#endif

#define SOCKET_GET_ERROR()      WSAGetLastError()
#define SOCKET_SET_ERROR(value) WSASetLastError(value)
#define SOCKET_ERRORNAME(name)  WSA##name

#define WSAEAGAIN WSAEWOULDBLOCK
#define WSAENOMEM WSA_NOT_ENOUGH_MEMORY

#else

#define closesocket close
#define ioctlsocket ioctl

#define SOCKET_GET_ERROR()      (errno)
#define SOCKET_SET_ERROR(value) (errno = (value))
#define SOCKET_ERRORNAME(name)  name

#endif

#endif
