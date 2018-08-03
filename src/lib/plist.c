#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "plist.h"

#define BPLIST_HEADER_LEN 8
#define BPLIST_TRAILER_LEN 32

typedef struct {
	uint64_t length;
	uint8_t *value;
} plist_data_t;

typedef struct {
	uint64_t size;
	plist_object_t **values;
} plist_array_t;

typedef struct {
	uint64_t size;
	char **keys;
	plist_object_t **values;
} plist_dict_t;

struct plist_object_s {
	uint8_t type;
	union {
		uint8_t       value_primitive;
		int64_t       value_integer;
		double        value_real;
		plist_data_t  value_data;
		char *        value_string;
		plist_array_t value_array;
		plist_dict_t  value_dict;
	} value;
};

static int
parse_integer(const uint8_t *data, uint64_t dataidx, uint8_t length, int64_t *value) {
	assert(data);
	assert(value);

	switch (length) {
	case 1:
		*value = data[dataidx++];
		break;
	case 2:
		*value = ((int64_t) data[dataidx++]) << 8;
		*value |= (int64_t) data[dataidx++];
		break;
	case 4:
		*value = ((int64_t) data[dataidx++]) << 24;
		*value |= ((int64_t) data[dataidx++]) << 16;
		*value |= ((int64_t) data[dataidx++]) << 8;
		*value |= (int64_t) data[dataidx++];
		break;
	case 8:
		*value = ((int64_t) data[dataidx++]) << 56;
		*value |= ((int64_t) data[dataidx++]) << 48;
		*value |= ((int64_t) data[dataidx++]) << 40;
		*value |= ((int64_t) data[dataidx++]) << 32;
		*value |= ((int64_t) data[dataidx++]) << 24;
		*value |= ((int64_t) data[dataidx++]) << 16;
		*value |= ((int64_t) data[dataidx++]) << 8;
		*value |= (int64_t) data[dataidx++];
		break;
	default:
		return -1;
	}
	return length;
}

static int
serialize_integer(uint8_t *data, uint64_t *dataidx, uint8_t length, int64_t value)
{
	switch (length) {
	case 1:
		data[(*dataidx)++] = (uint8_t) value;
		break;
	case 2:
		data[(*dataidx)++] = (uint8_t) (value >> 8);
		data[(*dataidx)++] = (uint8_t) value;
		break;
	case 4:
		data[(*dataidx)++] = (uint8_t) (value >> 24);
		data[(*dataidx)++] = (uint8_t) (value >> 16);
		data[(*dataidx)++] = (uint8_t) (value >> 8);
		data[(*dataidx)++] = (uint8_t) value;
		break;
	case 8:
		data[(*dataidx)++] = (uint8_t) (value >> 56);
		data[(*dataidx)++] = (uint8_t) (value >> 48);
		data[(*dataidx)++] = (uint8_t) (value >> 40);
		data[(*dataidx)++] = (uint8_t) (value >> 32);
		data[(*dataidx)++] = (uint8_t) (value >> 24);
		data[(*dataidx)++] = (uint8_t) (value >> 16);
		data[(*dataidx)++] = (uint8_t) (value >> 8);
		data[(*dataidx)++] = (uint8_t) value;
		break;
	default:
		return -1;
	}
	return length;
}

static int
parse_real(const uint8_t *data, uint64_t dataidx, uint64_t length, double *value)
{
	assert(data);
	assert(value);

	if (length == 4) {
		*value = *((float *) &data[dataidx]);
	} else {
		*value = *((double *) &data[dataidx]);
	}
	return length;
}

static uint8_t
integer_length(int64_t value) {
	if (value > 0 && value < (1 << 8)) {
		return 1;
	} else if (value > 0 && value < (1 << 16)) {
		return 2;
	} else if (value > 0 && value < ((int64_t) 1 << 32)) {
		return 4;
	} else {
		return 8;
	}
}

static uint8_t
blist_integer_length(int64_t value) {
	if (value > 0 && value < (1 << 8)) {
		return 0;
	} else if (value > 0 && value < (1 << 16)) {
		return 1;
	} else if (value > 0 && value < ((int64_t) 1 << 32)) {
		return 2;
	} else {
		return 3;
	}
}

static void
bplist_analyze(plist_object_t *object, uint64_t *objects, uint64_t *bytes, uint64_t *refs)
{
	int64_t count;
	int64_t i;

	*objects += 1;
	if (!object) {
		*bytes += 1;
		return;
	}
	if (object->type == PLIST_TYPE_PRIMITIVE) {
		*bytes += 1;
	} else if (object->type == PLIST_TYPE_INTEGER) {
		*bytes += 1+integer_length(object->value.value_integer);
	} else if (object->type == PLIST_TYPE_REAL) {
		*bytes += 1+8;
	} else if (object->type == PLIST_TYPE_DATA) {
		uint64_t length = object->value.value_data.length;
		if (length < 15) {
			*bytes += 1+length;
		} else {
			*bytes += 1+1+integer_length(length)+length;
		}
	} else if (object->type == PLIST_TYPE_STRING) {
		uint64_t length = strlen(object->value.value_string);
		if (length < 15) {
			*bytes += 1+length;
		} else {
			*bytes += 1+1+integer_length(length)+length;
		}
	} else if (object->type == PLIST_TYPE_ARRAY) {
		uint64_t size = object->value.value_array.size;
		if (size < 15) {
			*bytes += 1;
		} else {
			*bytes += 1+1+integer_length(size);
		}
		*refs += size;
		for (i=0; i<size; i++) {
			bplist_analyze(object->value.value_array.values[i], objects, bytes, refs);
		}
	} else if (object->type == PLIST_TYPE_DICT) {
		uint64_t size = object->value.value_dict.size;
		if (2*size < 15) {
			*bytes += 1;
		} else {
			*bytes += 1+1+integer_length(2*size);
		}
		*refs += 2*size;
		for (i=0; i<size; i++) {
			uint64_t keylen = strlen(object->value.value_dict.keys[i]);
			*objects += 1;
			if (keylen < 15) {
				*bytes += 1+keylen;
			} else {
				*bytes += 1+1+integer_length(keylen)+keylen;
			}
			bplist_analyze(object->value.value_dict.values[i], objects, bytes, refs);
		}
	}
}

static int64_t
bplist_serialize_string(int64_t *reftab, uint64_t *reftabidx, uint8_t *data, uint64_t *dataidx, char *value)
{
	int64_t length;
	int64_t objectid;

	objectid = (int64_t) *reftabidx;
	reftab[(*reftabidx)++] = *dataidx;

	length = (int64_t) strlen(value);
	if (length < 15) {
		data[(*dataidx)++] = PLIST_TYPE_STRING | length;
	} else {
		data[(*dataidx)++] = PLIST_TYPE_STRING | 0x0f;
		data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(length);
		serialize_integer(data, dataidx, integer_length(length), length);
	}
	memcpy(&data[*dataidx], value, length);
	*dataidx += length;

	return objectid;
}

static int64_t
bplist_serialize_object(int64_t *reftab, uint64_t *reftabidx, uint8_t reflen, uint8_t *data, uint64_t *dataidx, plist_object_t *object)
{
	int64_t objectid;
	uint64_t i;

	objectid = (int64_t) *reftabidx;
	reftab[(*reftabidx)++] = *dataidx;
	if (!object) {
		data[(*dataidx)++] = 0;
		return objectid;
	}
	if (object->type == PLIST_TYPE_PRIMITIVE) {
		data[(*dataidx)++] = PLIST_TYPE_PRIMITIVE | object->value.value_primitive;
	} else if (object->type == PLIST_TYPE_INTEGER) {
		int64_t value = object->value.value_integer;
		data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(value);
		serialize_integer(data, dataidx, integer_length(value), value);
	} else if (object->type == PLIST_TYPE_REAL) {
		data[(*dataidx)++] = PLIST_TYPE_REAL | 3;
		memcpy(&data[*dataidx], &object->value.value_real, sizeof(double));
		*dataidx += sizeof(double);
	} else if (object->type == PLIST_TYPE_DATA) {
		int64_t length = (int64_t) object->value.value_data.length;
		if (length < 15) {
			data[(*dataidx)++] = PLIST_TYPE_DATA | length;
		} else {
			data[(*dataidx)++] = PLIST_TYPE_DATA | 0x0f;
			data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(length);
			serialize_integer(data, dataidx, integer_length(length), length);
		}
		memcpy(&data[*dataidx], object->value.value_data.value, length);
		*dataidx += length;
	} else if (object->type == PLIST_TYPE_STRING) {
		int64_t length = (int64_t) strlen(object->value.value_string);
		if (length < 15) {
			data[(*dataidx)++] = PLIST_TYPE_STRING | length;
		} else {
			data[(*dataidx)++] = PLIST_TYPE_STRING | 0x0f;
			data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(length);
			serialize_integer(data, dataidx, integer_length(length), length);
		}
		memcpy(&data[*dataidx], object->value.value_string, length);
		*dataidx += length;
	} else if (object->type == PLIST_TYPE_ARRAY) {
		int64_t size = (int64_t) object->value.value_array.size;
		uint64_t valueidx;

		if (size < 15) {
			data[(*dataidx)++] = PLIST_TYPE_ARRAY | size;
		} else {
			data[(*dataidx)++] = PLIST_TYPE_ARRAY | 0x0f;
			data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(size);
			serialize_integer(data, dataidx, integer_length(size), size);
		}

		/* Reserve space for references */
		valueidx = *dataidx;
		*dataidx += size * reflen;
		for (i=0; i<size; i++) {
			int64_t valueid = bplist_serialize_object(reftab, reftabidx, reflen, data, dataidx, object->value.value_array.values[i]);
			serialize_integer(data, &valueidx, reflen, valueid);
		}
	} else if (object->type == PLIST_TYPE_DICT) {
		int64_t size = (int64_t) object->value.value_dict.size;
		uint64_t keyidx, valueidx;

		if (size < 15) {
			data[(*dataidx)++] = PLIST_TYPE_DICT | size;
		} else {
			data[(*dataidx)++] = PLIST_TYPE_DICT | 0x0f;
			data[(*dataidx)++] = PLIST_TYPE_INTEGER | blist_integer_length(size);
			serialize_integer(data, dataidx, integer_length(size), size);
		}
		keyidx = *dataidx;
		*dataidx += size * reflen;
		valueidx = *dataidx;
		*dataidx += size * reflen;
		for (i=0; i<size; i++) {
			int64_t keyid, valueid;

			keyid = bplist_serialize_string(reftab, reftabidx, data, dataidx, object->value.value_dict.keys[i]);
			valueid = bplist_serialize_object(reftab, reftabidx, reflen, data, dataidx, object->value.value_dict.values[i]);
			serialize_integer(data, &keyidx, reflen, keyid);
			serialize_integer(data, &valueidx, reflen, valueid);
		}
	}
	return objectid;
}

static plist_object_t *
bplist_parse_object(const int64_t *reftab, uint64_t reftablen, uint64_t reftabidx, const uint8_t *data, uint64_t datalen, uint8_t reflen)
{
	plist_object_t *object;
	uint64_t dataidx;
	uint8_t type;
	uint64_t length;
	int ret;

	if (reftabidx >= reftablen) {
		return NULL;
	}
	dataidx = reftab[reftabidx];
	if (dataidx >= datalen) {
		return NULL;
	}
	type = data[dataidx++];
	if ((type & 0x0f) < 15) {
		length = type & 0x0f;
	} else {
		uint8_t lentype;
		uint64_t lenlength;
		int64_t lenvalue;

		if (dataidx >= datalen) {
			return NULL;
		}
		lentype = data[dataidx++];
		if ((lentype & 0xf0) != PLIST_TYPE_INTEGER) {
			return NULL;
		}
		lenlength = (1 << lentype & 0x0f);
		if (dataidx + lenlength > datalen) {
			return NULL;
		}
		ret = parse_integer(data, dataidx, lenlength, &lenvalue);
		if (ret < 0 || lenvalue < 0) {
			return NULL;
		}
		length = lenvalue;
		dataidx += ret;
	}

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}

	object->type = type & 0xf0;
	if (object->type == PLIST_TYPE_PRIMITIVE) {
		object->value.value_primitive = type & 0x0f;
	} else if (object->type == PLIST_TYPE_INTEGER) {
		if (dataidx + (1 << length) > datalen) {
			free(object);
			return NULL;
		}
		ret = parse_integer(data, dataidx, (1 << length), &object->value.value_integer);
		if (ret < 0) {
			free(object);
			return NULL;
		}
	} else if (object->type == PLIST_TYPE_REAL) {
		if (dataidx + (1 << length) > datalen) {
			free(object);
			return NULL;
		}
		ret = parse_real(data, dataidx, (1 << length), &object->value.value_real);
		if (ret < 0) {
			free(object);
			return NULL;
		}
	} else if (object->type == PLIST_TYPE_DATA) {
		plist_data_t *plist_data;
		uint8_t *buffer;

		if (dataidx + length > datalen) {
			free(object);
			return NULL;
		}
		buffer = malloc(length);
		if (!buffer) {
			free(object);
			return NULL;
		}
		memcpy(buffer, data + dataidx, length);

		object->value.value_data.length = length;
		object->value.value_data.value = buffer;
	} else if (object->type == PLIST_TYPE_STRING) {
		char *buffer;

		if (dataidx + length > datalen) {
			free(object);
			return NULL;
		}
		buffer = calloc(length + 1, sizeof(char));
		if (!buffer) {
			free(object);
			return NULL;
		}
		memcpy(buffer, data + dataidx, length);
		object->value.value_string = buffer;
	} else if (object->type == PLIST_TYPE_ARRAY) {
		plist_object_t **values;
		uint64_t i;

		if (dataidx + length * reflen > datalen) {
			free(object);
			return NULL;
		}
		values = calloc(length, sizeof(plist_object_t *));
		if (!values) {
			free(object);
			return NULL;
		}
		for (i=0; i<length; i++) {
			int64_t valueidx;

			if (dataidx + reflen > datalen) {
				break;
			}
			ret = parse_integer(data, dataidx, reflen, &valueidx);
			if (ret < 0 || valueidx < 0) {
				break;
			}
			dataidx += reflen;
			values[i] = bplist_parse_object(reftab, reftablen, valueidx, data, datalen, reflen);
			if (!values[i]) {
				break;
			}
		}
		if (i != length) {
			for (i=0; i<length; i++) {
				plist_object_destroy(values[i]);
			}
			free(values);
			free(object);
			return NULL;
		}
		object->value.value_array.size = length;
		object->value.value_array.values = values;
	} else if (object->type == PLIST_TYPE_DICT) {
		char **keys;
		plist_object_t **values;
		uint64_t ki, vi;

		if (dataidx + 2 * length * reflen > datalen) {
			free(object);
			return NULL;
		}
		keys = calloc(length, sizeof(char *));
		if (!keys) {
			free(object);
			return NULL;
		}
		values = calloc(length, sizeof(plist_object_t *));
		if (!values) {
			free(keys);
			free(object);
			return NULL;
		}
		for (ki=0; ki<length; ki++) {
			int64_t keyidx;
			plist_object_t *obj;

			if (dataidx + reflen > datalen) {
				break;
			}
			ret = parse_integer(data, dataidx, reflen, &keyidx);
			if (ret < 0) {
				break;
			}
			dataidx += reflen;
			obj = bplist_parse_object(reftab, reftablen, keyidx, data, datalen, reflen);
			if (!obj) {
				break;
			}
			if (obj->type != PLIST_TYPE_STRING) {
				plist_object_destroy(obj);
				break;
			}
			keys[ki] = obj->value.value_string;
			free(obj);
		}
		for (vi=0; vi<length; vi++) {
			int64_t valueidx;

			if (dataidx + reflen > datalen) {
				break;
			}
			ret = parse_integer(data, dataidx, reflen, &valueidx);
			if (ret < 0) {
				break;
			}
			dataidx += reflen;
			values[vi] = bplist_parse_object(reftab, reftablen, valueidx, data, datalen, reflen);
			if (!values[vi]) {
				break;
			}
		}
		if (ki != length || vi != length) {
			uint64_t i;
			for (i=0; i<length; i++) {
				free(keys[i]);
				plist_object_destroy(values[i]);
			}
			free(values);
			free(keys);
			free(object);
			return NULL;
		}
		object->value.value_dict.size = length;
		object->value.value_dict.keys = keys;
		object->value.value_dict.values = values;
	} else {
		/* Currently unhandled type */
		free(object);
		return NULL;
	}

	return object;
}

plist_object_t *
plist_object_true()
{
	plist_object_t *object;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}

	object->type = PLIST_TYPE_PRIMITIVE;
	object->value.value_primitive = PLIST_PRIMITIVE_TRUE;

	return object;
}

plist_object_t *
plist_object_false()
{
	plist_object_t *object;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}

	object->type = PLIST_TYPE_PRIMITIVE;
	object->value.value_primitive = PLIST_PRIMITIVE_FALSE;

	return object;
}

plist_object_t *
plist_object_integer(uint64_t value)
{
	plist_object_t *object;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}

	object->type = PLIST_TYPE_INTEGER;
	object->value.value_integer = value;

	return object;
}

plist_object_t *
plist_object_real(double value)
{
	plist_object_t *object;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}

	object->type = PLIST_TYPE_REAL;
	object->value.value_real = value;

	return object;
}

plist_object_t *
plist_object_data(const uint8_t *value, uint32_t valuelen)
{
	plist_object_t *object;
	uint8_t *buffer;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}
	buffer = malloc(valuelen);
	if (!buffer) {
		free(object);
		return NULL;
	}
	memcpy(buffer, value, valuelen);

	object->type = PLIST_TYPE_DATA;
	object->value.value_data.value = buffer;
	object->value.value_data.length = valuelen;

	return object;
}

plist_object_t *
plist_object_string(const char *value)
{
	plist_object_t *object;
	uint64_t valuelen;
	char *buffer;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}
	valuelen = strlen(value);
	buffer = malloc(valuelen + 1);
	if (!buffer) {
		free(object);
		return NULL;
	}
	memcpy(buffer, value, valuelen + 1);

	object->type = PLIST_TYPE_STRING;
	object->value.value_string = buffer;

	return object;
}

plist_object_t *
plist_object_array(uint32_t size, ...)
{
	plist_object_t *object;
	plist_object_t **values;
	va_list ap;
	uint64_t i;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}
	values = calloc(size, sizeof(plist_object_t *));
	if (!values) {
		free(object);
		return NULL;
	}

	va_start(ap, size);
	for (i=0; i<size; i++) {
		values[i] = va_arg(ap, plist_object_t *);
	}
	va_end(ap);

	object->type = PLIST_TYPE_ARRAY;
	object->value.value_array.size = size;
	object->value.value_array.values = values;

	return object;
}

plist_object_t *
plist_object_dict(uint32_t size, ...)
{
	plist_object_t *object;
	char **keys;
	plist_object_t **values;
	va_list ap;
	uint64_t i;

	object = calloc(1, sizeof(plist_object_t));
	if (!object) {
		return NULL;
	}
	keys = calloc(size, sizeof(char *));
	if (!keys) {
		free(object);
		return NULL;
	}
	values = calloc(size, sizeof(plist_object_t *));
	if (!values) {
		free(keys);
		free(object);
		return NULL;
	}

	va_start(ap, size);
	for (i=0; i<size; i++) {
		const char *key = va_arg(ap, const char *);
		int keylen = strlen(key);

		keys[i] = calloc(keylen+1, sizeof(char));
		if (keys[i]) {
			memcpy(keys[i], key, keylen);
		}
		values[i] = va_arg(ap, plist_object_t *);
	}
	va_end(ap);

	object->type = PLIST_TYPE_DICT;
	object->value.value_dict.size = size;
	object->value.value_dict.keys = keys;
	object->value.value_dict.values = values;

	return object;
}

uint8_t
plist_object_get_type(plist_object_t *object)
{
	if (!object) {
		return 0xff;
	}
	return object->type;
}

int
plist_object_primitive_get_value(plist_object_t *object, uint8_t *value)
{
	if (!object || !value) {
		return -1;
	}
	if (object->type != PLIST_TYPE_PRIMITIVE) {
		return -2;
	}
	*value = object->value.value_primitive;
	return 0;
}

int
plist_object_integer_get_value(plist_object_t *object, int64_t *value)
{
	if (!object || !value) {
		return -1;
	}
	if (object->type != PLIST_TYPE_INTEGER) {
		return -2;
	}
	*value = object->value.value_integer;
	return 0;
}

int
plist_object_real_get_value(plist_object_t *object, double *value)
{
	if (!object || !value) {
		return -1;
	}
	if (object->type != PLIST_TYPE_REAL) {
		return -2;
	}
	*value = object->value.value_real;
	return 0;
}

int
plist_object_data_get_value(plist_object_t *object, const uint8_t **value, uint32_t *valuelen)
{
	if (!object || !value || !valuelen) {
		return -1;
	}
	if (object->type != PLIST_TYPE_DATA) {
		return -2;
	}
	*value = object->value.value_data.value;
	*valuelen = object->value.value_data.length;
	return 0;
}

int
plist_object_string_get_value(plist_object_t *object, const char **value)
{
	if (!object || !value) {
		return -1;
	}
	if (object->type != PLIST_TYPE_STRING) {
		return -2;
	}
	*value = object->value.value_string;
	return 0;
}

const plist_object_t *
plist_object_array_get_value(plist_object_t *object, uint32_t idx)
{
	if (!object) {
		return NULL;
	}
	if (object->type != PLIST_TYPE_ARRAY) {
		return NULL;
	}
	if (idx >= object->value.value_array.size) {
		return NULL;
	}
	return object->value.value_array.values[idx];
}

const plist_object_t *
plist_object_dict_get_value(plist_object_t *object, const char *key)
{
	int i;

	if (!object || !key) {
		return NULL;
	}
	if (object->type != PLIST_TYPE_DICT) {
		return NULL;
	}
	for (i=0; i<object->value.value_dict.size; i++) {
		if (!strcmp(key, object->value.value_dict.keys[i])) {
			return object->value.value_dict.values[i];
		}
	}
	return NULL;
}

plist_object_t *
plist_object_from_bplist(const uint8_t *data, uint32_t datalen)
{
	plist_object_t *object;
	const uint8_t *trailer;
	uint8_t offlen, reflen;
	int64_t objects, rootid, reftaboffset;
	int64_t *reftab;
	int i;
	
	if (!data) {
		return NULL;
	}
	if (datalen < BPLIST_TRAILER_LEN) {
		return NULL;
	}

	trailer = &data[datalen - BPLIST_TRAILER_LEN];
	offlen = trailer[6];
	reflen = trailer[7];
	parse_integer(trailer, 8, 8, &objects);
	parse_integer(trailer, 16, 8, &rootid);
	parse_integer(trailer, 24, 8, &reftaboffset);
	if (objects <= 0) {
		return NULL;
	}
	if (rootid < 0 || rootid >= objects) {
		return NULL;
	}
	if (reftaboffset < BPLIST_HEADER_LEN || reftaboffset + objects*offlen > datalen) {
		return NULL;
	}

	reftab = calloc(objects, sizeof(int64_t));
	if (!reftab) {
		return NULL;
	}
	for (i=0; i<objects; i++) {
		parse_integer(data, reftaboffset + i*offlen, offlen, &reftab[i]);
	}
	object = bplist_parse_object(reftab, objects, rootid, data, datalen, reflen);
	free(reftab);

	return object;
}

int
plist_object_to_bplist(plist_object_t *object, uint8_t **data, uint32_t *datalen)
{
	uint64_t objects, bytes, refs;
	uint8_t reflen, offlen;
	uint8_t *buf;
	uint32_t buflen;
	uint64_t bufidx;
	int64_t *reftab;
	uint64_t reftabidx;
	uint64_t reftaboffset;
	int i;

	if (!object || !data || !datalen) {
		return -1;
	}

	objects = bytes = refs = 0;
	bplist_analyze(object, &objects, &bytes, &refs);
	reflen = integer_length(refs);

	buflen = BPLIST_HEADER_LEN;
	buflen += bytes + refs * reflen;
	offlen = integer_length(buflen);
	buflen += objects * offlen;
	buflen += BPLIST_TRAILER_LEN;

	buf = calloc(buflen, sizeof(uint8_t));
	if (!buf) {
		return -2;
	}
	bufidx = 0;

	reftab = calloc(objects, sizeof(uint64_t));
	if (!reftab) {
		free(buf);
		return -3;
	}
	reftabidx = 0;

	memcpy(buf, "bplist00", BPLIST_HEADER_LEN);
	bufidx += BPLIST_HEADER_LEN;

	bplist_serialize_object(reftab, &reftabidx, reflen, buf, &bufidx, object);
	reftaboffset = bufidx;
	for (i=0; i<objects; i++) {
		serialize_integer(buf, &bufidx, offlen, reftab[i]);
	}

	bufidx += 6; /* Unused bytes in blist trailer */
	serialize_integer(buf, &bufidx, 1, offlen);
	serialize_integer(buf, &bufidx, 1, reflen);
	serialize_integer(buf, &bufidx, 8, objects);
	/* We always serialize root object as 0 */
	serialize_integer(buf, &bufidx, 8, 0);
	serialize_integer(buf, &bufidx, 8, reftaboffset);

	*data = buf;
	*datalen = buflen;
	return 0;
}

void
plist_object_destroy(plist_object_t *object)
{
	uint64_t i;
	if (!object) {
		return;
	}

	switch (object->type) {
	case PLIST_TYPE_DATA:
		free(object->value.value_data.value);
		break;
	case PLIST_TYPE_STRING:
		free(object->value.value_string);
		break;
	case PLIST_TYPE_ARRAY:
		for (i=0; i<object->value.value_array.size; i++) {
			plist_object_destroy(object->value.value_array.values[i]);
		}
		free(object->value.value_array.values);
		break;
	case PLIST_TYPE_DICT:
		for (i=0; i<object->value.value_dict.size; i++) {
			free(object->value.value_dict.keys[i]);
		}
		free(object->value.value_dict.keys);
		for (i=0; i<object->value.value_dict.size; i++) {
			plist_object_destroy(object->value.value_dict.values[i]);
		}
		free(object->value.value_dict.values);
		break;
	}
	free(object);
}

#ifdef MAIN
#include <stdio.h>

#define SAMPLE_BPLIST "\x62\x70\x6c\x69\x73\x74\x30\x30\xd7\x01\x03\x05\x07\x09\x0b\x0d\x02\x04\x06\x08\x0a\x0c\x0e\x54\x74\x72\x75\x65\x08\x55\x66\x61\x6c\x73\x65\x09\x57\x69\x6e\x74\x65\x67\x65\x72\x12\x00\xbc\x61\x4e\x54\x72\x65\x61\x6c\x23\x5d\x1d\x5b\x2a\xca\xc0\xf3\x3f\x54\x64\x61\x74\x61\x44\x64\x61\x74\x61\x56\x73\x74\x72\x69\x6e\x67\x5c\x73\x74\x72\x69\x6e\x67\x20\x76\x61\x6c\x75\x65\x55\x61\x72\x72\x61\x79\xa2\x0f\x10\x55\x66\x69\x72\x73\x74\x56\x73\x65\x63\x6f\x6e\x64\x08\x17\x1c\x1d\x23\x24\x2c\x31\x36\x3f\x44\x49\x50\x5d\x63\x66\x6c\x00\x00\x00\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x11\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x73"

static void
test_decode()
{
	plist_object_t *object;
	const uint8_t *indata;
	uint32_t indatalen;
	plist_object_t *intobj;
	int64_t intval;
	uint8_t *outdata;
	uint32_t outdatalen;
	int i;

	indata = (const uint8_t *) SAMPLE_BPLIST;
	indatalen = sizeof(SAMPLE_BPLIST)-1;

	object = plist_object_from_bplist(indata, indatalen);
	if (!object) {
		printf("Error parsing bplist data\n");
		return;
	}
	intobj = plist_object_dict_get_value(object, "integer");
	plist_object_integer_get_value(intobj, &intval);
	printf("Integer value: %d\n", (int) intval);

	plist_object_to_bplist(object, &outdata, &outdatalen);
	printf("Parsed and serialized bplist: ");
	for (i=0; i<outdatalen; i++) {
		printf("\\x%02x", outdata[i]);
	}
	printf("\n");

	plist_object_destroy(object);
	free(outdata);
}

static void
test_encode()
{
	uint8_t *data;
	uint32_t datalen;
	int i;

	plist_object_t *obj = plist_object_dict(7,
		"true", plist_object_true(),
		"false", plist_object_false(),
		"integer", plist_object_integer(12345678),
		"real", plist_object_real(1.2345678),
		"data", plist_object_data((uint8_t *) "data", 4),
		"string", plist_object_string("string value"),
		"array", plist_object_array(2,
			plist_object_string("first"),
			plist_object_string("second")
		)
	);
	plist_object_to_bplist(obj, &data, &datalen);
	printf("Serialized bplist: ");
	for (i=0; i<datalen; i++) {
		printf("\\x%02x", data[i]);
	}
	printf("\n");

	plist_object_destroy(obj);
	free(data);
}

int
main(int argc, char*argv[])
{
	test_decode();
	test_encode();
}
#endif
