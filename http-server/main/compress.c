/*
	Example compression and decompression using zlib.

	This example code is in the Public Domain (or CC0 licensed, at your option.)
	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "zlib.h"

#include "parameter.h"

// I used this code.
// https://www.zlib.net/zpipe.c

//#define CHUNK 16384
#define CHUNK 2048

/* Compress from file source to file dest until EOF on source.
   comp() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int comp(FILE *source, FILE *dest, int level)
{
	int ret, flush;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, level);
	if (ret != Z_OK) {
		ESP_LOGE(__FUNCTION__, "deflateInit fail [%d]", ret);
		return ret;
	}

	/* compress until end of file */
	do {
		strm.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source)) {
			(void)deflateEnd(&strm);
			ESP_LOGE(__FUNCTION__, "fread fail");
			return Z_ERRNO;
		}
		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		/* run deflate() on input until output buffer not full, finish
		   compression if all of source has been read in */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = deflate(&strm, flush);	/* no bad return value */
			assert(ret != Z_STREAM_ERROR);	/* state not clobbered */
			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				(void)deflateEnd(&strm);
				ESP_LOGE(__FUNCTION__, "fwrite fail");
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);
		assert(strm.avail_in == 0);		/* all input will be used */

		/* done when last data in file processed */
	} while (flush != Z_FINISH);
	assert(ret == Z_STREAM_END);		/* stream will be complete */

	/* clean up and return */
	(void)deflateEnd(&strm);
	return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   decomp() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int decomp(FILE *source, FILE *dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK) {
		ESP_LOGE(__FUNCTION__, "inflateInit fail [%d]", ret);
		return ret;
	}

	/* decompress until deflate stream ends or end of file */
	do {
		strm.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source)) {
			(void)inflateEnd(&strm);
			ESP_LOGE(__FUNCTION__, "fread fail");
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);	/* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;		/* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				(void)inflateEnd(&strm);
				ESP_LOGE(__FUNCTION__, "fwrite fail");
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

void comp_task(void *pvParameters) {
	PARAMETER_t *task_parameter = pvParameters;
	PARAMETER_t param;
	memcpy((char *)&param, task_parameter, sizeof(PARAMETER_t));
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	ESP_LOGI(pcTaskGetName(NULL), "param.srcPath=[%s]", param.srcPath);
	ESP_LOGI(pcTaskGetName(NULL), "param.dstPath=[%s]", param.dstPath);

	// Compress
	FILE* src = fopen(param.srcPath, "r");
	FILE* dst = fopen(param.dstPath, "w");
	int result = comp(src, dst, Z_DEFAULT_COMPRESSION);
	if (result != 0) ESP_LOGE(pcTaskGetName(NULL), "comp=%d", result);
	fclose(src);
	fclose(dst);
	xTaskNotify( param.ParentTaskHandle, result, eSetValueWithOverwrite );
	ESP_LOGI(pcTaskGetName(NULL), "Finish");
	vTaskDelete( NULL );
}
	
void decomp_task(void *pvParameters) {
	PARAMETER_t *task_parameter = pvParameters;
	PARAMETER_t param;
	memcpy((char *)&param, task_parameter, sizeof(PARAMETER_t));
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	ESP_LOGI(pcTaskGetName(NULL), "param.srcPath=[%s]", param.srcPath);
	ESP_LOGI(pcTaskGetName(NULL), "param.dstPath=[%s]", param.dstPath);

	// DeCompress
	FILE* src = fopen(param.srcPath, "r");
	FILE* dst = fopen(param.dstPath, "w");
	int result = decomp(src, dst);
	if (result != 0) ESP_LOGI(pcTaskGetName(NULL), "decomp=%d", result);
	fclose(src);
	fclose(dst);
	xTaskNotify( param.ParentTaskHandle, result, eSetValueWithOverwrite );
	ESP_LOGI(pcTaskGetName(NULL), "Finish");
	vTaskDelete( NULL );
}
