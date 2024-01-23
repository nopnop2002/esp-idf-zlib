/* MQTT (over TCP) Example

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
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h" // esp_base_mac_addr_get
#include "mqtt_client.h"
#include "esp_rom_md5.h"
#include "esp_vfs.h"

#include "queue.h"
#include "mqtt.h"

static const char *TAG = "PUB";

extern QueueHandle_t xQueuePublish;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
#endif
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_event_handle_t event = event_data;
#else
#endif
	QUEUE_t queueBuf;
	queueBuf.request = REQUEST_MQTT;
	queueBuf.event_id = event->event_id;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xQueueSendFromISR(xQueuePublish, &queueBuf, NULL);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			xQueueSendFromISR(xQueuePublish, &queueBuf, NULL);
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			xQueueSendFromISR(xQueuePublish, &queueBuf, NULL);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			xQueueSendFromISR(xQueuePublish, &queueBuf, NULL);
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
	return ESP_OK;
#endif
}

esp_err_t query_mdns_host(const char * host_name, char *ip);
void convert_mdns_host(char * from, char * to);

#define MQTT_PUT_RESPONSE "/mqtt/files/put/res"
#define MQTT_GET_RESPONSE "/mqtt/files/get/res"
#define MQTT_LIST_RESPONSE "/mqtt/files/list/res"
#define MQTT_DELETE_RESPONSE "/mqtt/files/delete/res"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

void wait_for(esp_mqtt_client_handle_t mqtt_client, int32_t event_id)
{
	QUEUE_t queueBuf;
	xQueueReceive(xQueuePublish, &queueBuf, portMAX_DELAY);
	if (queueBuf.request == REQUEST_MQTT) {
		if (queueBuf.event_id == event_id) return;
	}
}

void send_header(esp_mqtt_client_handle_t mqtt_client, char *topic, char *file, int file_len, char *md5, int md5_len)
{
	char buffer[128];
	memset(buffer, 0x2C, 128); // fill kanma
	memcpy(&buffer[0], "header", 6);
	memcpy(&buffer[8], file, file_len);
	if (md5_len > 0) {
		memcpy(&buffer[10+file_len], md5, md5_len);
	}
	esp_mqtt_client_publish(mqtt_client, topic, buffer, 128, 1, 0);
	// wait MQTT_EVENT_PUBLISHED
	wait_for(mqtt_client, MQTT_EVENT_PUBLISHED);
}

void send_tailer(esp_mqtt_client_handle_t mqtt_client, char *topic, char *file, int file_len, char *md5, int md5_len)
{
	char buffer[128];
	memset(buffer, 0x2C, 128); // fill kanma
	memcpy(&buffer[0], "tailer", 6);
	memcpy(&buffer[8], file, file_len);
	memcpy(&buffer[10+file_len], md5, md5_len);
	esp_mqtt_client_publish(mqtt_client, topic, buffer, 128, 1, 0);
	// wait MQTT_EVENT_PUBLISHED
	wait_for(mqtt_client, MQTT_EVENT_PUBLISHED);
}

void mqtt_pub(void *pvParameters)
{
	char *mount_point = (char *)pvParameters;
	ESP_LOGI(TAG, "Start mount_point=[%s]", mount_point);
	ESP_LOGI(TAG, "CONFIG_MQTT_BROKER=[%s]", CONFIG_MQTT_BROKER);

	// Set client id from mac
	uint8_t mac[8];
	ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	for(int i=0;i<8;i++) {
		ESP_LOGD(TAG, "mac[%d]=%x", i, mac[i]);
	}
	char client_id[64];
	sprintf(client_id, "pub-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	ESP_LOGI(TAG, "client_id=[%s]", client_id);

	// Resolve mDNS host name
	char ip[128];
	ESP_LOGI(TAG, "CONFIG_MQTT_BROKER=[%s]", CONFIG_MQTT_BROKER);
	convert_mdns_host(CONFIG_MQTT_BROKER, ip);
	ESP_LOGI(TAG, "ip=[%s]", ip);
	char uri[138];
	sprintf(uri, "mqtt://%s", ip);
	ESP_LOGI(TAG, "uri=[%s]", uri);

	// Initialize mqtt
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = uri,
		.broker.address.port = 1883,
#if CONFIG_BROKER_AUTHENTICATION
		.credentials.username = CONFIG_AUTHENTICATION_USERNAME,
		.credentials.authentication.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.credentials.client_id = client_id
	};
#else
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = uri,
		.port = 1883,
		.event_handle = mqtt_event_handler,
#if CONFIG_BROKER_AUTHENTICATION
		.username = CONFIG_AUTHENTICATION_USERNAME,
		.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.client_id = client_id
	};
#endif

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
#endif

	esp_mqtt_client_start(mqtt_client);

	QUEUE_t queueBuf;
	char filepath[FILE_PATH_MAX];
	struct stat filestat;
	char md5[33];
	FILE *file = NULL;
	//int fileOpen = 0;
	int fileSize = 0;
	struct MD5Context context;
	while (1) {
		xQueueReceive(xQueuePublish, &queueBuf, portMAX_DELAY);
		ESP_LOGI(TAG, "request=%d", queueBuf.request);
		if (queueBuf.request == REQUEST_MQTT) {
			ESP_LOGI(TAG, "MQTT event_id=%"PRIi32, queueBuf.event_id);
			if (queueBuf.event_id == MQTT_EVENT_CONNECTED) {
				ESP_LOGI(TAG, "coonect to MQTT Server");
			} else if (queueBuf.event_id == MQTT_EVENT_DISCONNECTED) {
				break;
			} else if (queueBuf.event_id == MQTT_EVENT_ERROR) {
				break;
			}
		} // REQUEST_MQTT

		if (queueBuf.request == REQUEST_PUT) {
			ESP_LOGI(TAG, "REQUEST_PUT. payload=[%.*s]", queueBuf.payload_len, queueBuf.payload);
			ESP_LOGI(TAG, "queueBuf.responce=%d", queueBuf.responce);
			if (queueBuf.responce == RESPONCE_ACK) {
				send_header(mqtt_client, MQTT_PUT_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_ACK_PAYLOAD, strlen(RESPONCE_ACK_PAYLOAD));
			} else {
				send_header(mqtt_client, MQTT_PUT_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_NAK_PAYLOAD, strlen(RESPONCE_NAK_PAYLOAD));
			}
			send_tailer(mqtt_client, MQTT_PUT_RESPONSE, queueBuf.payload, queueBuf.payload_len, "", 0);

		} // REQUEST_PUT

		if (queueBuf.request == REQUEST_GET) {
			ESP_LOGI(TAG, "REQUEST_GET. payload=[%.*s]", queueBuf.payload_len, queueBuf.payload);
			strcpy(filepath, mount_point);
			strcat(filepath, "/");
			strncat(filepath, queueBuf.payload, queueBuf.payload_len);
			ESP_LOGI(TAG, "filepath=[%s]", filepath);
			
			int stat_status = stat(filepath, &filestat);
			ESP_LOGI(TAG, "stat_status=%d", stat_status);
			//if (stat(filepath, &filestat) != 0) {
			if (stat_status != 0) {
				ESP_LOGE(TAG, "%s not found", filepath);
				send_header(mqtt_client, MQTT_GET_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_NAK_PAYLOAD, strlen(RESPONCE_NAK_PAYLOAD));
				memset(md5, 0x00, sizeof(md5));
				memset(md5, 0x30, sizeof(md5)-1);
				send_tailer(mqtt_client, MQTT_GET_RESPONSE, queueBuf.payload, queueBuf.payload_len, md5, strlen(md5));
				continue;
			}

			ESP_LOGI(TAG, "%s found", filepath);
			// Get file size
			if (stat(filepath, &filestat) == -1) {
				ESP_LOGE(TAG, "Failed to stat %s", filepath);
				continue;
			}
			ESP_LOGI(TAG, "filesize=[%ld]", filestat.st_size);

			send_header(mqtt_client, MQTT_GET_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_ACK_PAYLOAD, strlen(RESPONCE_ACK_PAYLOAD));
			esp_rom_md5_init(&context);
			file = fopen(filepath, "rb");
			if (file == NULL) {
				ESP_LOGE(TAG, "Failed to open file for writing");
			}
			unsigned char data[MQTT_PAYLOAD_MAX];
			fileSize = 0;
			int postCount = 1;
			while(1) {
				int ret = fread(data, 1, MQTT_PAYLOAD_MAX, file);
				ESP_LOGI(TAG, "fread rer=%d", ret);
				if (ret == 0) break;
				esp_mqtt_client_publish(mqtt_client, MQTT_GET_RESPONSE, (char *)data, ret, 1, 0);
				// wait MQTT_EVENT_PUBLISHED
				wait_for(mqtt_client, MQTT_EVENT_PUBLISHED);
				esp_rom_md5_update(&context, data, ret);
				fileSize = fileSize + ret;
				ESP_LOGI(TAG, "%d/%ld", fileSize, filestat.st_size);
				postCount++;
				vTaskDelay(MQTT_DELAY_TICK);
			}
			ESP_LOGI(TAG, "fileSize=%d", fileSize);
			fclose(file);
			// Get md5
			unsigned char digest[16];
			esp_rom_md5_final(digest, &context);
			// Convert md5 to hex character
			char hexdigest[33];
			memset(hexdigest, 0, sizeof(hexdigest));
			for(int i=0;i<16;i++) {
				char work[3];
				snprintf(work, 3, "%02x", digest[i]);
				strcat(hexdigest, work);
			}
			ESP_LOGI(TAG, "hexdigest=[%s]", hexdigest);
			send_tailer(mqtt_client, MQTT_GET_RESPONSE, queueBuf.payload, queueBuf.payload_len, hexdigest, strlen(hexdigest));
			postCount++;
			ESP_LOGI(TAG, "postCount=%d",postCount);

		} // REQUEST_GET

		if (queueBuf.request == REQUEST_LIST) {
			ESP_LOGI(TAG, "REQUEST_LIST");
			DIR *dir = opendir(mount_point);
			if (!dir) {
				ESP_LOGE(TAG, "Failed to stat dir : %s", mount_point);
				send_header(mqtt_client, MQTT_LIST_RESPONSE, "list", 4, RESPONCE_NAK_PAYLOAD, strlen(RESPONCE_NAK_PAYLOAD));
			} else {
				send_header(mqtt_client, MQTT_LIST_RESPONSE, "list", 4, RESPONCE_ACK_PAYLOAD, strlen(RESPONCE_ACK_PAYLOAD));
				struct dirent *entry;
				while ((entry = readdir(dir)) != NULL) {
					ESP_LOGI(TAG, "entry->d_name=[%s]", entry->d_name);

					strcpy(filepath, mount_point);
					strcat(filepath, "/");
					strcat(filepath, entry->d_name);
					ESP_LOGI(TAG, "filepath=[%s]", filepath);
			
					// Get file size
					struct stat filestat;
					if (stat(filepath, &filestat) == -1) {
						ESP_LOGE(TAG, "Failed to stat %s", filepath);
						continue;
					}
					ESP_LOGI(TAG, "filesize=[%ld]", filestat.st_size);

					char buffer[128];
					sprintf(buffer, "%.*s %ld", FILE_PATH_MAX, entry->d_name, filestat.st_size);
					//esp_mqtt_client_publish(mqtt_client, MQTT_LIST_RESPONSE, entry->d_name, 0, 1, 0);
					esp_mqtt_client_publish(mqtt_client, MQTT_LIST_RESPONSE, buffer, 0, 1, 0);
					// wait MQTT_EVENT_PUBLISHED
					wait_for(mqtt_client, MQTT_EVENT_PUBLISHED);
				}
				closedir(dir);
			}
			send_tailer(mqtt_client, MQTT_LIST_RESPONSE, "list", 4, "", 0);

		} // REQUEST_LIST

		if (queueBuf.request == REQUEST_DELETE) {
			ESP_LOGI(TAG, "REQUEST_DELETE. payload=[%.*s]", queueBuf.payload_len, queueBuf.payload);
			strcpy(filepath, mount_point);
			strcat(filepath, "/");
			strncat(filepath, queueBuf.payload, queueBuf.payload_len);
			ESP_LOGI(TAG, "filepath=[%s]", filepath);
			
			struct stat filestat;
			int stat_status = stat(filepath, &filestat);
			ESP_LOGI(TAG, "stat_status=%d", stat_status);
			if (stat_status != 0) {
				ESP_LOGE(TAG, "%s not found", filepath);
				send_header(mqtt_client, MQTT_DELETE_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_NAK_PAYLOAD, strlen(RESPONCE_NAK_PAYLOAD));
			} else {
				ESP_LOGI(TAG, "%s found", filepath);
				send_header(mqtt_client, MQTT_DELETE_RESPONSE, queueBuf.payload, queueBuf.payload_len, RESPONCE_ACK_PAYLOAD, strlen(RESPONCE_ACK_PAYLOAD));
				unlink(filepath);
			}
			memset(md5, 0x00, sizeof(md5));
			memset(md5, 0x30, sizeof(md5)-1);
			send_tailer(mqtt_client, MQTT_DELETE_RESPONSE, queueBuf.payload, queueBuf.payload_len, md5, strlen(md5));

		} // REQUEST_DELETE
	}

	// Stop mqtt
	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
