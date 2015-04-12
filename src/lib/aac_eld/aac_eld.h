#ifndef __AAC__ELD_DECOMP_H
#define __AAC__ELD_DECOMP_H

typedef struct aac_eld_file aac_eld_file;

aac_eld_file *create_aac_eld();
void aac_eld_decode_frame(aac_eld_file *aac_eld,
                  unsigned char *inbuffer, int inputsize,
                  void *outbuffer, int *outputsize);
void aac_eld_set_info(aac_eld_file *aac_eld, char *inputbuffer);
void destroy_aac_eld(aac_eld_file *aac_eld);

#endif /* __ALAC__DECOMP_H */

