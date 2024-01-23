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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "zlib.h"

#include "parameter.h"

static const char *TAG = "MAIN";

esp_err_t mountSPIFFS(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
		.base_path = mount_point,
		.partition_label = partition_label,
		.max_files = 4, // maximum number of files which can be open at the same time
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

static int getFileSize(char *fullPath) {
	struct stat st;
	if (stat(fullPath, &st) == 0)
		return st.st_size;
	return -1;
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
			int fsize = getFileSize(fullPath);
			ESP_LOGI(__FUNCTION__,"%s d_name=%s d_ino=%d fsize=%d", path, pe->d_name, pe->d_ino, fsize);
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

void comp_task(void *pvParameters);
void decomp_task(void *pvParameters);

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

	// Mount SPIFFS File System on FLASH
	char *mount_point = "/root";
	char *partition_label = "storage";
	ESP_ERROR_CHECK(mountSPIFFS(partition_label, mount_point));

	// Compress
	printDirectory(mount_point);
	PARAMETER_t param;
	param.ParentTaskHandle =  xTaskGetCurrentTaskHandle();
	strcpy(param.srcPath, "/root/README.txt");
	strcpy(param.dstPath, "/root/README.txt.zlib");
	param.level = Z_DEFAULT_COMPRESSION;
	UBaseType_t priority = uxTaskPriorityGet(NULL);
	xTaskCreate(comp_task, "COMPRESS", 1024*6, (void *)&param, priority, NULL);
	uint32_t comp_result = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
	ESP_LOGI(TAG, "comp_result=%"PRIi32, comp_result);
	if (comp_result != 0) vTaskDelete(NULL);
	printDirectory(mount_point);

	// Decompress
	strcpy(param.srcPath, "/root/README.txt.zlib");
	strcpy(param.dstPath, "/root/README.txt.txt");
	xTaskCreate(decomp_task, "DECOMPRESS", 1024*6, (void *)&param, priority, NULL);
	uint32_t decomp_result = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
	ESP_LOGI(TAG, "decomp_result=%"PRIi32, decomp_result);
	printDirectory(mount_point);

	// Unmount SPIFFS File System
	esp_vfs_spiffs_unregister(partition_label);
	ESP_LOGI(TAG, "FLASH unmounted");
}
