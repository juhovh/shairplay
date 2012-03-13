#ifndef RSAPEM_H
#define RSAPEM_H

typedef struct rsapem_s rsapem_t;

rsapem_t *rsapem_init(const char *pemstr);
int rsapem_read_vector(rsapem_t *rsapem, unsigned char **data);
void rsapem_destroy(rsapem_t *rsapem);

#endif
