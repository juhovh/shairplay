/*
 * decode AAC-ELD audio data from mac by XBMC, and play it by SDL
 *
 * modify:
 * 2012-10-31   first version (ffmpeg tutorial03.c)
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <fdk-aac/aacdecoder_lib.h>
#include "aac_eld.h"
 
/* ---------------------------------------------------------- */
/*          next n lines is libfdk-aac config                 */
/* ---------------------------------------------------------- */
 
/* period size 480 samples */  
#define N_SAMPLE 480  
/* ASC config binary data */
unsigned char eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
unsigned char *conf[] = { eld_conf };                   //TODO just for aac eld config
static unsigned int conf_len = sizeof(eld_conf);

static int pcm_pkt_size = 4 * N_SAMPLE;

struct aac_eld_file {
	int fdk_flags;
	HANDLE_AACDECODER phandle;
	TRANSPORT_TYPE transportFmt; 
	unsigned int nrOfLayers; 
	CStreamInfo *stream_info;
};
 
/*
 * create aac eld decoder
 */
aac_eld_file *create_aac_eld(void)
{
    int ret = 0;
    aac_eld_file *aac;
 
    aac = malloc(sizeof(aac_eld_file));
    if (!aac) return NULL;

    aac->fdk_flags = 0;
    aac->transportFmt = 0; //raw
    aac->nrOfLayers = 1; // 1 layer
    aac->phandle = aacDecoder_Open(aac->transportFmt, aac->nrOfLayers);
    if (aac->phandle == NULL) {
        printf("aacDecoder open faild!\n");
	return NULL;
    }
 
    printf("conf_len = %d\n", conf_len);
    ret = aacDecoder_ConfigRaw(aac->phandle, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        fprintf(stderr, "Unable to set configRaw\n");
	return NULL;
    }
 
    aac->stream_info = aacDecoder_GetStreamInfo(aac->phandle);
    if (aac->stream_info == NULL) {
        printf("aacDecoder_GetStreamInfo failed!\n");
	return NULL;
    }
    printf("> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d\n",   \
            aac->stream_info->channelConfig, aac->stream_info->aacSampleRate,
            aac->stream_info->aacSamplesPerFrame, aac->stream_info->aot, aac->stream_info->bitRate);
    return aac;
}

void destroy_aac_eld(aac_eld_file *aac)
{
	if (aac) {
    		aacDecoder_Close(aac->phandle);
        	free(aac);
	}
}

void aac_eld_set_info(aac_eld_file *aac_eld, char *inputbuffer)
{
}
 
/*
 * called by external, aac data input queue
 */
void aac_eld_decode_frame(aac_eld_file *aac_eld, unsigned char *inbuffer, int inputsize, void *outbuffer, int *outputsize)
{
    int ret = 0;
    unsigned char *input_buf[1];
    int sizes[1];
    int valid_size;

    input_buf[0] = inbuffer;
    sizes[0] = inputsize;
 
    /* step 1 -> fill aac_data_buf to decoder's internal buf */
    ret = aacDecoder_Fill(aac_eld->phandle, input_buf, sizes, &valid_size);
    if (ret != AAC_DEC_OK) {
        fprintf(stderr, "Fill failed: %x\n", ret);
        *outputsize  = 0;
        return;
    }
 
    /* step 2 -> call decoder function */
    ret = aacDecoder_DecodeFrame(aac_eld->phandle, outbuffer, pcm_pkt_size, aac_eld->fdk_flags);
    if (ret != AAC_DEC_OK) {
        fprintf(stderr, "aacDecoder_DecodeFrame : 0x%x\n", ret);
        *outputsize  = 0;
        return;
    }

    *outputsize = pcm_pkt_size;

    /* TOCHECK: need to check and handle inputsize != valid_size ? */
    fprintf(stderr, "pcm output %d\n", *outputsize);
}
 
