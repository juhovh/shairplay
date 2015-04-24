/**
 *  Copyright (C) 2014  Joakim Plate
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

#ifndef RAOP_NTP_H
#define RAOP_NTP_H

#include "logger.h"

#define RAOP_NTP_COUNT   8
#define RAOP_NTP_PHI_PPM   15ull                   /* PPM */
#define RAOP_NTP_R_RHO   ((1ull    << 32) / 1000u) /* packet precision       */
#define RAOP_NTP_S_RHO   ((1ull    << 32) / 1000u) /* system clock precision */
#define RAOP_NTP_MAXDIST ((1500ull << 32) / 1000u) /* maximum allowed distance */
#define RAOP_NTP_MAXDISP ((16ull   << 32))         /* maximum dispersion */

#define RAOP_NTP_CLOCK_BASE (2208988800ull << 32)


typedef struct raop_ntp_data_s {
	unsigned long long clock;
	unsigned long long dispersion;
	long long          delay;
	long long          offset;
} raop_ntp_data_t;

typedef struct raop_ntp_s {
	logger_t *          logger;
	unsigned long long  transmit;
	raop_ntp_data_t     data[RAOP_NTP_COUNT];
	int                 index;
} raop_ntp_t;


void raop_ntp_init(raop_ntp_t *ntp, logger_t *logger, unsigned long long clock);
void raop_ntp_get_offset(raop_ntp_t *ntp, unsigned long long clock, long long *offset, unsigned long long *dispersion);
void raop_ntp_add(raop_ntp_t* ntp, unsigned long long origin, unsigned long long receive, unsigned long long transmit, unsigned long long current);

#endif
