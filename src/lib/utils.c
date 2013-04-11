/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *
utils_strsep(char **stringp, const char *delim)
{
	char *original;
	char *strptr;

	if (*stringp == NULL) {
		return NULL;
	}

	original = *stringp;
	strptr = strstr(*stringp, delim);
	if (strptr == NULL) {
		*stringp = NULL;
		return original;
	}
	*strptr = '\0';
	*stringp = strptr+strlen(delim);
	return original;
}

int
utils_read_file(char **dst, const char *filename)
{
	FILE *stream;
	int filesize;
	char *buffer;
	int read_bytes;

	/* Open stream for reading */
	stream = fopen(filename, "rb");
	if (!stream) {
		return -1;
	}

	/* Find out file size */
	fseek(stream, 0, SEEK_END);
	filesize = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	/* Allocate one extra byte for zero */
	buffer = malloc(filesize+1);
	if (!buffer) {
		fclose(stream);
		return -2;
	}

	/* Read data in a loop to buffer */
	read_bytes = 0;
	do {
		int ret = fread(buffer+read_bytes, 1,
		                filesize-read_bytes, stream);
		if (ret == 0) {
			break;
		}
		read_bytes += ret;
	} while (read_bytes < filesize);

	/* Add final null byte and close stream */
	buffer[read_bytes] = '\0';
	fclose(stream);

	/* If read didn't finish, return error */
	if (read_bytes != filesize) {
		free(buffer);
		return -3;
	}

	/* Return buffer */
	*dst = buffer;
	return filesize;
}

