/*
 * grease_log.c
 *
 *  Created on: Apr 2, 2015
 *      Author: ed
 * (c) 2015, WigWag Inc.
 */

#include <stdint.h>

#include "grease_client.h"

//static uint32_t grease_PREAMBLE = SINK_LOG_PREAMBLE;
const logMeta __noMetaData = {
		.tag = 0,
		.level = 0,
		.origin = 0,
		.target = 0 };

const uint32_t __grease_preamble = SINK_LOG_PREAMBLE;

/**
 * create a log entry for use across the network to Grease.
 * Memory layout: [PREAMBLE][Length (type RawLogLen)][logMeta][logdata - string - no null termination]
 * @param f a pointer to a meta data for logging. If NULL, it will be empty meta data
 * @param s string to log
 * @param len length of the passed in string
 * @param tobuf A raw buffer to store the log output ready for processing
 * @param len A pointer to an int. This will be read to know the existing length of the buffer, and then set
 * the size of the buffer that should be sent
 * @return returns GREASE_OK if successful, or GREASE_NO_BUFFER if the buffer is too small. If parameters are
 * invalid returns GREASE_INVALID_PARAMS
 */
int logToRaw(logMeta *f, char *s, RawLogLen len, char *tobuf, int *buflen) {
	if(!tobuf || *buflen < (GREASE_RAWBUF_MIN_SIZE + len))  // sanity check
		return GREASE_NO_BUFFER;
	int w = 0;

	memcpy(tobuf,&__grease_preamble,SIZEOF_SINK_LOG_PREAMBLE);
	w += SIZEOF_SINK_LOG_PREAMBLE;
	memcpy(tobuf+w,*buflen,sizeof(RawLogLen));
	w += sizeof(RawLogLen);
	if(f)
		memcpy(tobuf+w,f,sizeof(logMeta));
	else
		memcpy(tobuf+w,&__noMetaData,sizeof(logMeta));
	w += sizeof(logMeta);
	if(s && len > 0) {
		memcpy(tobuf+w,s,len);
	}
	*buflen = len;
	return GREASE_OK;
}



