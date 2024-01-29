/*
	BSD Socket TCP Server Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_vfs.h"
#include "zlib.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "parameter.h"

static const char *TAG = "TCP";
#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 3
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

#define ZLIB_EXTENSION ".zlib"

int getFileSize(char *fullPath) {
	struct stat st;
	if (stat(fullPath, &st) == 0)
		return st.st_size;
	return -1;
}

void printDirectory(char * path) {
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

int receive_socket(int sock, char *rx_buffer, int rx_buffer_size, int flag) {
	int len;
	char packet_length;

	// Receive packet length
	len = recv(sock, &packet_length, 1, flag);
	ESP_LOGD(__FUNCTION__, "len=%d packet_length=%d", len, packet_length);
	if (len <= 0) return len;

	// Receive packet body
	int index = 0;
	int remain_size = packet_length;
	while(1) {
		len = recv(sock, &rx_buffer[index], remain_size, flag);
		index = index + len;
		remain_size = remain_size - len;
		ESP_LOGD(__FUNCTION__, "len=%d index=%d remain_size=%d", len, index, remain_size);
		if (remain_size == 0) break;
	}
	return index;
}

// File copy from Host to SPIFFS
void put_file(int sock, char *filepath) {
	char rx_buffer[RX_BUFFER_SIZE];
	char tx_buffer[TX_BUFFER_SIZE];
	char md5[33];
	FILE *file = NULL;
	int fileOpen = 0;
	int fileSize = 0;
	struct MD5Context context;

	while (1) {
		//int rx_length = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		int rx_length = receive_socket(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		// Error occurred during receiving
		if (rx_length < 0) {
			ESP_LOGE(__FUNCTION__, "recv failed: errno %d", errno);
			break;
		}

		// Data received
		ESP_LOGI(__FUNCTION__, "Received %d bytes", rx_length);
		//ESP_LOGI(__FUNCTION__, "[%s]", rx_buffer);

		if (strncmp(rx_buffer, "header", 6) == 0) {
			ESP_LOGI(__FUNCTION__, "[%.*s]", rx_length, rx_buffer);
			// Create file
			ESP_LOGI(__FUNCTION__, "filepath=[%s]", filepath);
			file = fopen(filepath, "wb");
			if (file == NULL) {
				ESP_LOGE(__FUNCTION__, "Failed to open file for writing");
				strcpy(tx_buffer, "NG");
			} else {
				fileOpen = 1;
				fileSize = 0;
				esp_rom_md5_init(&context);
				strcpy(tx_buffer, "OK");
			}
			int err = send(sock, tx_buffer, strlen(tx_buffer), 0);
			if (err < 0) {
				ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
				break;
			}
		} // end header

		else if (strncmp(rx_buffer, "tailer", 6) == 0) {
			ESP_LOGI(__FUNCTION__, "[%.*s]", rx_length, rx_buffer);
			memset(md5, 0, sizeof(md5));
			strncpy(md5, &rx_buffer[8], 32);
			ESP_LOGI(__FUNCTION__, "md5=[%s]", md5);
			if (fileOpen) {
				// Close file
				fclose(file);
				ESP_LOGI(__FUNCTION__, "fileSize=%d", fileSize);
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
				//printDirectory(mount_point);
				ESP_LOGI(__FUNCTION__, "hexdigest=[%s]", hexdigest);
				// Compare md5
				if (strcmp(md5, hexdigest) == 0) {
					ESP_LOGI(__FUNCTION__, "File copied OK - valid hash");
					strcpy(tx_buffer, "OK");
				} else {
					ESP_LOGE(__FUNCTION__, "File copied NG - invalid hash");
					strcpy(tx_buffer, "NG");
				}
				file = NULL;
				fileOpen = 0;
			} else {
				strcpy(tx_buffer, "NG");
			}
			int err = send(sock, tx_buffer, strlen(tx_buffer), 0);
			if (err < 0) {
				ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
				break;
			}
			break;
		} // end tailer


		else {
			if (fileOpen) {
				// Write file
				int ret = fwrite(rx_buffer, rx_length, 1, file);
				if (ret != 1) {
					ESP_LOGE(__FUNCTION__, "Failed to write file");
					strcpy(tx_buffer, "NG");
				} else {
					// Update md5
					fileSize = fileSize + rx_length;
					esp_rom_md5_update(&context, rx_buffer, rx_length);
					strcpy(tx_buffer, "OK");
				}
			}
			int err = send(sock, tx_buffer, strlen(tx_buffer), 0);
			if (err < 0) {
				ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
				break;
			}
		} // end data

	} // end while
}

int send_packet(int sock, char *tx_buffer, int tx_length) {
	char packet_length[1];
	packet_length[0] = tx_length;
	int err = send(sock, packet_length, 1, 0);
	if (err < 0) {
		ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
		return err;
	}

	err = send(sock, tx_buffer, tx_length, 0);
	if (err < 0) {
		ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
		return err;
	}

	// Waiting response
	char rx_buffer[TX_BUFFER_SIZE];
	int rx_length = receive_socket(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
	ESP_LOGI(__FUNCTION__, "rx_length=%d", rx_length);
	// Error occurred during receiving
	if (rx_length < 0) {
		ESP_LOGE(__FUNCTION__, "recv failed: errno %d", errno);
		return rx_length;
	}
	return 0;
}

int send_header(int sock, int filesize)
{
	char tx_buffer[128];
	memset(tx_buffer, 0x2C, 128); // fill canma
	memcpy(&tx_buffer[0], "header", 6);
	char filesizeChar[16];
	int filesizeLen = sprintf(filesizeChar, "%d", filesize);
	memcpy(&tx_buffer[8], filesizeChar, filesizeLen);

	send_packet(sock, tx_buffer, 128);
	return 0;
}

int send_tailer(int sock, char *md5)
{
	char tx_buffer[128];
	memset(tx_buffer, 0x2C, 128); // fill canma
	memcpy(&tx_buffer[0], "tailer", 6);
	memcpy(&tx_buffer[8], md5, strlen(md5));

	send_packet(sock, tx_buffer, 128);
	return 0;
}

// File copy from SPIFFS to Host
void get_file(int sock, char *filepath) {
	struct stat st;
	if (stat(filepath, &st) != 0) {
		ESP_LOGE(__FUNCTION__, "file not found [%s]", filepath);
		return;
	}
	
	send_header(sock, st.st_size);
	FILE *file = fopen(filepath, "rb");
	if (file == NULL) {
		ESP_LOGE(__FUNCTION__, "Failed to open file for writing");
		return;
	}

	char tx_buffer[RX_BUFFER_SIZE];
	struct MD5Context context;
	esp_rom_md5_init(&context);
	int fileSize = 0;
	while(1) {
		int ret = fread(tx_buffer, 1, RX_BUFFER_SIZE-1, file);
		ESP_LOGI(TAG, "fread rer=%d", ret);
		if (ret == 0) break;
		send_packet(sock, tx_buffer, ret);
		esp_rom_md5_update(&context, tx_buffer, ret);
		fileSize = fileSize + ret;
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
	send_tailer(sock, hexdigest);
}

// Delete file from SPIFFS
void del_file(int sock, char *filepath) {
	struct stat st;
	if (stat(filepath, &st) != 0) {
		ESP_LOGE(__FUNCTION__, "file not found [%s]", filepath);
		return;
	}
	ESP_LOGI(__FUNCTION__, "unlink [%s]", filepath);
	unlink(filepath);
}

void comp_task(void *pvParameters);

void receive_command(struct sockaddr_in6 source_addr, int sock, char *mount_point) {
	char rx_buffer[RX_BUFFER_SIZE];
	char tx_buffer[TX_BUFFER_SIZE];
	char filepath[FILE_PATH_MAX];
	char addr_str[128];

	while (1) {
		//int rx_length = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		int rx_length = receive_socket(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		// Error occurred during receiving
		if (rx_length < 0) {
			ESP_LOGE(__FUNCTION__, "recv failed: errno %d", errno);
			break;
		}
		// Connection closed by client
		else if (rx_length == 0) {
			ESP_LOGI(__FUNCTION__, "Connection closed");
			break;
		}
		// Data received
		// Get the sender's ip address as string
		if (source_addr.sin6_family == PF_INET) {
			inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
		} else if (source_addr.sin6_family == PF_INET6) {
			inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
		}

		// Null-terminate whatever we received and treat like a string
		rx_buffer[rx_length] = 0;
		ESP_LOGI(__FUNCTION__, "Received %d bytes from %s:", rx_length, addr_str);
		ESP_LOGI(__FUNCTION__, "[%s]", rx_buffer);

		strcpy(tx_buffer, "OK");
		int err = send(sock, tx_buffer, strlen(tx_buffer), 0);
		if (err < 0) {
			ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
			break;
		}

		char *adr = strchr(rx_buffer, (int)',');
		ESP_LOGI(__FUNCTION__, "filename=[%s]", adr+1);
		memset(filepath, 0, sizeof(filepath));
		strcpy(filepath, mount_point);
		strcat(filepath, "/");
		strcat(filepath, adr+1);
		ESP_LOGI(__FUNCTION__, "filepath=[%s]", filepath);

		// Receive file from Host
		if (strncmp(rx_buffer, "put_file", 8) == 0) {
			put_file(sock, filepath);
			printDirectory(mount_point);

		// Send file to Host
		} else if (strncmp(rx_buffer, "get_file", 8) == 0) {
			get_file(sock, filepath);

		// Delete file from SPIFFS
		} else if (strncmp(rx_buffer, "del_file", 8) == 0) {
			printDirectory(mount_point);
			del_file(sock, filepath);
			printDirectory(mount_point);

		// Compress file on SPIFFS
		} else if (strncmp(rx_buffer, "compress", 8) == 0) {
			printDirectory(mount_point);
			PARAMETER_t param;
			param.ParentTaskHandle =  xTaskGetCurrentTaskHandle();
			strcpy(param.srcPath, filepath);
			strcpy(param.dstPath, param.srcPath);
			strcat(param.dstPath, ZLIB_EXTENSION);
			param.level = Z_DEFAULT_COMPRESSION;
			UBaseType_t priority = uxTaskPriorityGet(NULL);
			xTaskCreate(comp_task, "COMPRESS", 1024*6, (void *)&param, priority, NULL);
			uint32_t comp_result = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
			ESP_LOGI(TAG, "comp_result=%"PRIi32, comp_result);
			if (comp_result != 0) vTaskDelete(NULL);
			printDirectory(mount_point);

		}

	} // end while
}

void tcp_server(void *pvParameters)
{
	char *mount_point = (char *)pvParameters;
	ESP_LOGI(TAG, "Start mount_point=[%s] CONFIG_TCP_PORT=%d", mount_point, CONFIG_TCP_PORT);
	char addr_str[128];
	int addr_family;
	int ip_protocol;

#if 1
	struct sockaddr_in dest_addr;
	dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr.sin_family = AF_INET;
	//dest_addr.sin_port = htons(PORT);
	dest_addr.sin_port = htons(CONFIG_TCP_PORT);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;
	inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else
	struct sockaddr_in6 dest_addr;
	bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
	dest_addr.sin6_family = AF_INET6;
	dest_addr.sin6_port = htons(CONFIG_TCP_PORT);
	addr_family = AF_INET6;
	ip_protocol = IPPROTO_IPV6;
	inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

	int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
	if (listen_sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		return;
	}
	ESP_LOGI(TAG, "Socket created");

	int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0) {
		ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		return;
	}
	ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_TCP_PORT);

	err = listen(listen_sock, 1);
	if (err != 0) {
		ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
		vTaskDelete(NULL);
	}
	ESP_LOGI(TAG, "Socket listening");

	while(1) {
		struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
		socklen_t addr_len = sizeof(source_addr);
		int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
		if (sock < 0) {
			ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket accepted");

		receive_command(source_addr, sock, mount_point);

		ESP_LOGI(TAG, "Close socket");
		close(sock);
	} // end while

	vTaskDelete(NULL);
}
