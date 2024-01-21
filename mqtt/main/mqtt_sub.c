/*
	MQTT (over TCP) Example

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
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "esp_rom_md5.h"
#include "esp_vfs.h"

#include "queue.h"
#include "mqtt.h"

typedef struct {
	TaskHandle_t taskHandle;
	int32_t event_id;
	int topic_len;
	char topic[MQTT_TOPIC_MAX+1];
	int data_len;
	unsigned char data[MQTT_PAYLOAD_MAX+1];
} MQTT_t;

static const char *TAG = "SUB";

extern QueueHandle_t xQueueSubscribe;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
#endif
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_event_handle_t event = event_data;
	MQTT_t *mqttBuf = handler_args;
#else
	MQTT_t *mqttBuf = event->user_context;
#endif

	ESP_LOGD(TAG, "taskHandle=0x%x", (unsigned int)mqttBuf->taskHandle);
	mqttBuf->event_id = event->event_id;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			if (event->topic_len > MQTT_TOPIC_MAX) {
				ESP_LOGE(TAG, "event->topic_len [%d] exceeds MQTT_TOPIC_MAX", event->topic_len);
				break;
			}
			if (event->data_len > MQTT_PAYLOAD_MAX) {
				ESP_LOGE(TAG, "event->data_len [%d] exceeds MQTT_PAYLOAD_MAX", event->data_len);
				break;
			}
			//ESP_LOGI(TAG, "TOPIC=[%.*s] DATA=[%.*s]\r", event->topic_len, event->topic, event->data_len, event->data);
			ESP_LOGI(TAG, "TOPIC=[%.*s] event->data_len=%d", event->topic_len, event->topic, event->data_len);

			mqttBuf->topic_len = event->topic_len;
			for(int i=0;i<event->topic_len;i++) {
				mqttBuf->topic[i] = event->topic[i];
				mqttBuf->topic[i+1] = 0;
			}
			mqttBuf->data_len = event->data_len;
			for(int i=0;i<event->data_len;i++) {
				mqttBuf->data[i] = event->data[i];
				mqttBuf->data[i+1] = 0;
			}
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
	return ESP_OK;
#endif
}

typedef enum {HEADER, TAILER, DATA} TYPE;

#define MQTT_PUT_REQUEST "/mqtt/files/put/req"
#define MQTT_GET_REQUEST "/mqtt/files/get/req"
#define MQTT_LIST_REQUEST "/mqtt/files/list/req"
#define MQTT_DELETE_REQUEST "/mqtt/files/delete/req"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

int getFileSize(char *fullPath);
void printDirectory(char * path);
esp_err_t query_mdns_host(const char * host_name, char *ip);
void convert_mdns_host(char * from, char * to);

int process_message(unsigned char *data, int length, char *filename, char *md5) {
	if (length != 128) return  DATA;
	if (strncmp((char *)data, "header", 6) == 0) {
		for (int index=8;index<length;index++) {
			if (data[index] == ',') break;
			filename[index-8] = data[index];
			filename[index-7] = 0;
		}
		ESP_LOGD(TAG, "filename=[%s]", filename);
		return HEADER;
	} 
	if (strncmp((char *)data, "tailer", 6) == 0) {
		for (int index=8;index<length;index++) {
			if (data[index] == ',') {
				//strncpy(md5, &data[index+2], 32);
				strncpy(md5, (char *)&data[index+2], 32);
				md5[32] = 0;
				break;
			}
		}
		ESP_LOGD(TAG, "md5=[%s]", md5);
		return TAILER;
	} 
	return DATA;
}

void mqtt_sub(void *pvParameters)
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
	sprintf(client_id, "sub-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
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
	MQTT_t mqttBuf;
	mqttBuf.taskHandle = xTaskGetCurrentTaskHandle();
	ESP_LOGI(TAG, "taskHandle=0x%x", (unsigned int)mqttBuf.taskHandle);
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
		.user_context = &mqttBuf,
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
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, &mqttBuf);
#endif

	esp_mqtt_client_start(mqtt_client);

	QUEUE_t queueBuf;
	char filename[64];
	char filepath[FILE_PATH_MAX];
	char md5[33];
	FILE *file = NULL;
	int fileOpen = 0;
	int fileSize = 0;
	struct MD5Context context;
	while (1) {
		// Wait for mqtt event
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		ESP_LOGI(TAG, "ulTaskNotifyTake");
		ESP_LOGI(TAG, "event_id=%"PRIi32, mqttBuf.event_id);
		if (mqttBuf.event_id == MQTT_EVENT_CONNECTED) {
			ESP_LOGI(TAG, "coonect to MQTT Server");
			esp_mqtt_client_subscribe(mqtt_client, MQTT_PUT_REQUEST, 0);
			esp_mqtt_client_subscribe(mqtt_client, MQTT_GET_REQUEST, 0);
			esp_mqtt_client_subscribe(mqtt_client, MQTT_LIST_REQUEST, 0);
			esp_mqtt_client_subscribe(mqtt_client, MQTT_DELETE_REQUEST, 0);
		} else if (mqttBuf.event_id == MQTT_EVENT_DISCONNECTED) {
			break;
		} else if (mqttBuf.event_id == MQTT_EVENT_ERROR) {
			break;
		} else if (mqttBuf.event_id == MQTT_EVENT_DATA) {
			ESP_LOGI(TAG, "TOPIC=[%.*s]\r", mqttBuf.topic_len, mqttBuf.topic);
			//ESP_LOGI(TAG, "DATA=[%.*s]\r", mqttBuf.data_len, mqttBuf.data);

			if (strncmp(mqttBuf.topic, MQTT_PUT_REQUEST, mqttBuf.topic_len) == 0) {
				int type = process_message(mqttBuf.data, mqttBuf.data_len, filename, md5);
				if (type == HEADER) {
					// Create file
					ESP_LOGI(TAG, "filename=[%s]", filename);
					strcpy(filepath, mount_point);
					strcat(filepath, "/");
					strcat(filepath, filename);
					ESP_LOGI(TAG, "filepath=[%s]", filepath);
					file = fopen(filepath, "wb");
					if (file == NULL) {
						ESP_LOGE(TAG, "Failed to open file for writing");
					} else {
						fileOpen = 1;
						fileSize = 0;
						esp_rom_md5_init(&context);
					}
				} // end HEADER

				else if (type == TAILER) {
					ESP_LOGI(TAG, "md5=[%s]", md5);
					if (fileOpen) {
						// Close file
						fclose(file);
						ESP_LOGI(TAG, "fileSize=%d", fileSize);
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
						printDirectory(mount_point);
						ESP_LOGI(TAG, "hexdigest=[%s]", hexdigest);
						// Compare md5
						queueBuf.request = REQUEST_PUT;
						queueBuf.payload_len = strlen(filepath);
						strcpy(queueBuf.payload, filepath);
						if (strcmp(md5, hexdigest) == 0) {
							queueBuf.responce = RESPONCE_ACK;
							ESP_LOGI(TAG, "File copied OK - valid hash");
						} else {
							queueBuf.responce = RESPONCE_NAK;
							ESP_LOGE(TAG, "File copied NG - invalid hash");
						}
						// Post to main task
						if (xQueueSend(xQueueSubscribe, &queueBuf, 10) != pdPASS) {
							ESP_LOGE(TAG, "xQueueSend fail");
						}
						file = NULL;
						fileOpen = 0;
					}
				} // TAILER

				else {
					if (fileOpen) {
						// Write file
						int ret = fwrite(mqttBuf.data, mqttBuf.data_len, 1, file);
						if (ret != 1) {
							ESP_LOGE(TAG, "Failed to write file");
						} else {
							// Update md5
							fileSize = fileSize + mqttBuf.data_len;
							esp_rom_md5_update(&context, mqttBuf.data, mqttBuf.data_len);
						}
					}
				} // DATA

			} // MQTT_PUT_REQUEST

			if (strncmp(mqttBuf.topic, MQTT_GET_REQUEST, mqttBuf.topic_len) == 0) {
				process_message(mqttBuf.data, mqttBuf.data_len, filename, md5);
				ESP_LOGI(TAG, "filename=[%s]", filename);
				// Post to main task
				queueBuf.request = REQUEST_GET;
				queueBuf.payload_len = strlen(filename);
				strcpy(queueBuf.payload, filename);
				if (xQueueSend(xQueueSubscribe, &queueBuf, 10) != pdPASS) {
					ESP_LOGE(TAG, "xQueueSend fail");
				}
			} // MQTT_GET_REQUEST

			if (strncmp(mqttBuf.topic, MQTT_LIST_REQUEST, mqttBuf.topic_len) == 0) {
				// Post to main task
				queueBuf.request = REQUEST_LIST;
				queueBuf.payload_len = 0;
				if (xQueueSend(xQueueSubscribe, &queueBuf, 10) != pdPASS) {
					ESP_LOGE(TAG, "xQueueSend fail");
				}
			} // MQTT_LIST_REQUEST

			if (strncmp(mqttBuf.topic, MQTT_DELETE_REQUEST, mqttBuf.topic_len) == 0) {
				process_message(mqttBuf.data, mqttBuf.data_len, filename, md5);
				ESP_LOGI(TAG, "filename=[%s]", filename);
				// Post to main task
				queueBuf.request = REQUEST_DELETE;
				queueBuf.payload_len = strlen(filename);
				strcpy(queueBuf.payload, filename);
				if (xQueueSend(xQueueSubscribe, &queueBuf, 10) != pdPASS) {
					ESP_LOGE(TAG, "xQueueSend fail");
				}
			} // MQTT_DELETE_REQUEST
		}

	} // end while

	// Stop mqtt
	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
