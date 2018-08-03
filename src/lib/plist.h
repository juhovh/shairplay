#ifndef PLIST_H
#define PLIST_H

#if defined(_WIN32) && defined(_MSC_VER)
  typedef signed __int8    int8_t;
  typedef unsigned __int8  uint8_t;
  typedef __int16          int16_t;
  typedef unsigned __int16 uint16_t;
  typedef __int32          int32_t;
  typedef unsigned __int32 uint32_t;
  typedef __int64          int64_t;
  typedef unsigned __int64 uint64_t;
#else
  #include <stdint.h>
#endif

#define PLIST_TYPE_PRIMITIVE   0x00
#define PLIST_TYPE_INTEGER     0x10
#define PLIST_TYPE_REAL        0x20
#define PLIST_TYPE_DATA        0x40
#define PLIST_TYPE_STRING      0x50
#define PLIST_TYPE_ARRAY       0xA0
#define PLIST_TYPE_DICT        0xD0

#define PLIST_PRIMITIVE_TRUE  0x08
#define PLIST_PRIMITIVE_FALSE 0x09

typedef struct plist_object_s plist_object_t;

plist_object_t *plist_object_true();
plist_object_t *plist_object_false();
plist_object_t *plist_object_integer(uint64_t value);
plist_object_t *plist_object_real(double value);
plist_object_t *plist_object_data(const uint8_t *value, uint32_t valuelen);
plist_object_t *plist_object_string(const char *value);
plist_object_t *plist_object_array(uint32_t size, ...);
plist_object_t *plist_object_dict(uint32_t size, ...);

uint8_t plist_object_get_type(plist_object_t *object);
int plist_object_primitive_get_value(plist_object_t *object, uint8_t *value);
int plist_object_integer_get_value(plist_object_t *object, int64_t *value);
int plist_object_real_get_value(plist_object_t *object, double *value);
int plist_object_data_get_value(plist_object_t *object, const uint8_t **value, uint32_t *valuelen);
int plist_object_string_get_value(plist_object_t *object, const char **value);
const plist_object_t *plist_object_array_get_value(plist_object_t *object, uint32_t idx);
const plist_object_t *plist_object_dict_get_value(plist_object_t *object, const char *key);

plist_object_t *plist_object_from_bplist(const uint8_t *data, uint32_t datalen);
int plist_object_to_bplist(plist_object_t *object, uint8_t **data, uint32_t *datalen);

void plist_object_destroy(plist_object_t *object);

#endif
