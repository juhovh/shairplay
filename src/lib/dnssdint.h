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

#ifndef DNSSDINT_H
#define DNSSDINT_H

#define RAOP_TXTVERS "1"
#define RAOP_CH "2"             /* Audio channels: 2 */
#define RAOP_CN "0,1"           /* Audio codec: PCM, ALAC */
#define RAOP_ET "0,1"           /* Encryption type: none, RSA */
#define RAOP_SV "false"
#define RAOP_DA "true"
#define RAOP_SR "44100"
#define RAOP_SS "16"            /* Sample size: 16 */
#define RAOP_VN "3"
#define RAOP_TP "TCP,UDP"
#define RAOP_MD "0,1,2"         /* Metadata: text, artwork, progress */
#define RAOP_SM "false"
#define RAOP_EK "1"

#endif
