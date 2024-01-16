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
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "zlib.h"

//When using the FAT file system, enable the following line.
//#define USE_FAT

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define esp_vfs_fat_spiflash_mount esp_vfs_fat_spiflash_mount_rw_wl
#define esp_vfs_fat_spiflash_unmount esp_vfs_fat_spiflash_unmount_rw_wl
#endif

static const char *TAG = "MAIN";

esp_err_t mountSPIFFS(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");
	esp_vfs_spiffs_conf_t conf = {
		//.base_path = "/spiffs",
		.base_path = mount_point,
		//.partition_label = NULL,
		.partition_label = partition_label,
		.max_files = 4, // maximum number of files which can be open at the same time
		//.max_files = 256,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	//ret = esp_spiffs_info(NULL, &total, &used);
	ret = esp_spiffs_info(partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
	return ret;
}

wl_handle_t mountFAT(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing FAT file system");
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 4, // maximum number of files which can be open at the same time
		.allocation_unit_size = CONFIG_WL_SECTOR_SIZE
	};
	wl_handle_t s_wl_handle;
	esp_err_t err = esp_vfs_fat_spiflash_mount(mount_point, partition_label, &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FLASH (%s)", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Mount FAT filesystem on %s", mount_point);
	ESP_LOGI(TAG, "s_wl_handle=%"PRIi32, s_wl_handle);
	return s_wl_handle;
}

static fpos_t getFileSize(char *fullPath) {
	fpos_t fsize = 0;

	FILE *fp = fopen(fullPath,"rb");
	ESP_LOGD(TAG, "fullPath=[%s] fp=%p", fullPath, fp);
	if (fp == NULL) return 0;
	fseek(fp,0,SEEK_END);
	fgetpos(fp,&fsize);
	fclose(fp);
	ESP_LOGD(TAG, "fgetpos fsize=%ld", fsize);
	return fsize;
}

static void printDirectory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent *pe = readdir(dir);
		if (!pe) break;
		if (pe->d_type == 1) {
			char fullPath[64];
			strcpy(fullPath, path);
			strcat(fullPath, "/");
			strcat(fullPath, pe->d_name);
			fpos_t fsize = getFileSize(fullPath);
			ESP_LOGI(__FUNCTION__,"%s d_name=%s d_ino=%d fsize=%"PRIi32, path, pe->d_name, pe->d_ino, fsize);
		}
		if (pe->d_type == 2) {
			char subDir[127];
			sprintf(subDir,"%s%.64s", path, pe->d_name);
			ESP_LOGI(TAG, "subDir=[%s]", subDir);
			printDirectory(subDir);

		}
	}
	closedir(dir);
}

void app_main(void)
{
	ESP_LOGI(TAG, "MAX_MEM_LEVEL=%d", MAX_MEM_LEVEL);
	// Initialize NVS
	esp_err_t ret;
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	char *mount_point = "/root";
#ifdef USE_FAT
	// Mount FAT File System on FLASH
	char *partition_label = "storage0";
	wl_handle_t s_wl_handle = mountFAT(partition_label, mount_point);
	if (s_wl_handle < 0) {
		while(1) { vTaskDelay(1); }
	};
#else
	// Mount SPIFFS File System on FLASH
	char *partition_label = "storage1";
	ret = mountSPIFFS(partition_label, mount_point);
	if (ret != ESP_OK) {
		while(1) { vTaskDelay(1); }
	};
#endif
	

	// Compress
	char fullPath[64];
	strcpy(fullPath, mount_point);
	strcat(fullPath, "/out.gz");

	// Check if destination file exists before writing
	struct stat st;
	if (stat(fullPath, &st) == 0) {
		// Delete it if it exists
		unlink(fullPath);
	}

	char * data = "test test test\n";
	int dataSize = strlen(data);
	int outSize = 0;
	gzFile outFile = gzopen(fullPath,"wb");
	for (int i=0;i<10;i++) {
		gzwrite(outFile,data,strlen(data));
		outSize = outSize + dataSize;
	}
	gzclose(outFile);
	printDirectory(mount_point);
	fpos_t fileSize = getFileSize(fullPath);
	ESP_LOGI(TAG, "outSize=%d", outSize);
	ESP_LOGI(TAG, "fileSize=%"PRIi32, fileSize);

	// UnCompress
	gzFile inFile = gzopen(fullPath,"rb");
	int inSize = 0;
	char buf[64];
	while((ret = gzread(inFile, buf, sizeof(buf))) != 0 && ret != -1) {
		inSize += ret;
		ESP_LOGI(TAG, "buf=[%.*s]", ret, buf);
	}
	gzclose(inFile);
	ESP_LOGI(TAG, "inSize=%d", inSize);

#ifdef USE_FAT
	// Unmount FAT file system
	esp_vfs_fat_spiflash_unmount(mount_point, s_wl_handle);
#else
	// Unmount SPIFFS file system
	esp_vfs_spiffs_unregister(partition_label);
#endif
	ESP_LOGI(TAG, "FLASH unmounted");

}
