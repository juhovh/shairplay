/**
 *  Copyright (C) 2012  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef DNSSD_H
#define DNSSD_H

#if defined(WIN32) && defined(DLL_EXPORT)
# define DNSSD_API __declspec(dllexport)
#else
# define DNSSD_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DNSSD_ERROR_NOERROR       0
#define DNSSD_ERROR_HWADDRLEN     1
#define DNSSD_ERROR_OUTOFMEM      2
#define DNSSD_ERROR_LIBNOTFOUND   3
#define DNSSD_ERROR_PROCNOTFOUND  4

typedef struct dnssd_s dnssd_t;

DNSSD_API dnssd_t *dnssd_init(int *error);

DNSSD_API int dnssd_register_raop(dnssd_t *dnssd, const char *name, unsigned short port, const char *hwaddr, int hwaddrlen, int password);
DNSSD_API int dnssd_register_airplay(dnssd_t *dnssd, const char *name, unsigned short port, const char *hwaddr, int hwaddrlen);

DNSSD_API void dnssd_unregister_raop(dnssd_t *dnssd);
DNSSD_API void dnssd_unregister_airplay(dnssd_t *dnssd);

DNSSD_API void dnssd_destroy(dnssd_t *dnssd);

#ifdef __cplusplus
}
#endif
#endif
