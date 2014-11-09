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

#include "raop_ntp.h"
#include <string.h>
#include <stdlib.h>

void
raop_ntp_init(raop_ntp_t *ntp, logger_t *logger, unsigned long long clock)
{
	int i;
	ntp->logger = logger;
	for(i = 0; i < RAOP_NTP_COUNT; ++i) {
		ntp->data[i].offset     = 0ll;
		ntp->data[i].delay      = RAOP_NTP_MAXDISP;
		ntp->data[i].dispersion = RAOP_NTP_MAXDISP;
		ntp->data[i].clock      = clock;
	}
}

static int
raop_ntp_compare(const void* av, const void* bv)
{
	const raop_ntp_data_t* a = (const raop_ntp_data_t*)av;
	const raop_ntp_data_t* b = (const raop_ntp_data_t*)bv;
	if(a->delay < b->delay) {
		return -1;
	} else if(a->delay > b->delay) {
		return 1;
	} else {
		return 0;
	}
}

void
raop_ntp_get_offset(raop_ntp_t *ntp, unsigned long long clock, long long *offset, unsigned long long *dispersion)
{
	raop_ntp_data_t data[RAOP_NTP_COUNT];
	const unsigned  two_pow_n[RAOP_NTP_COUNT] = {2, 4, 8, 16, 32, 64, 128, 256};
	int i;

	/* sort by delay */
	memcpy(data, ntp->data, sizeof(data));
	qsort(data, RAOP_NTP_COUNT, sizeof(data[0]), raop_ntp_compare);

	*dispersion = 0ull;
	*offset     = ntp->data[0].offset;

	/* calculate dispersion */
	for(i = 0; i < RAOP_NTP_COUNT; ++i) {
		unsigned long long disp = ntp->data[i].dispersion + (clock - ntp->data[i].clock) * RAOP_NTP_PHI_PPM / 1000000u;
		*dispersion += disp / two_pow_n[i];
	}

	logger_log(ntp->logger, LOGGER_DEBUG, "Get ntp offset %f %f"
	                                         , (double)*dispersion / (1ll<<32)
	                                         , (double)*offset     / (1ll<<32));
}

void
raop_ntp_add(raop_ntp_t* ntp, unsigned long long origin, unsigned long long receive, unsigned long long transmit, unsigned long long current)
{
	ntp->index = (ntp->index + 1) % RAOP_NTP_COUNT;
	ntp->data[ntp->index].clock      = current;
	ntp->data[ntp->index].offset     = ((long long)(receive - origin) + (long long)(transmit - current)) / 2;
	ntp->data[ntp->index].delay      = ((long long)(current - origin) - (long long)(transmit - receive));
	ntp->data[ntp->index].dispersion = RAOP_NTP_R_RHO + RAOP_NTP_S_RHO +  (current - origin) * RAOP_NTP_PHI_PPM / 1000000u;
	logger_log(ntp->logger, LOGGER_DEBUG, "Add ntp data %f %f %f %f %f %f"
	                                         , (double)origin      / (1ll<<32)
	                                         , (double)receive     / (1ll<<32)
	                                         , (double)transmit    / (1ll<<32)
	                                         , (double)current     / (1ll<<32)
	                                         , (double)ntp->data[ntp->index].offset / (1ll<<32)
	                                         , (double)ntp->data[ntp->index].delay  / (1ll<<32));
}
