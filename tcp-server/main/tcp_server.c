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
#define RX_BUFFER_SIZE 2048
#define TX_BUFFER_SIZE 2
#define BLOCK_SIZE 128
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
			char subDir[128];
			sprintf(subDir,"%s%.64s", path, pe->d_name);
			ESP_LOGI(TAG, "subDir=[%s]", subDir);
			printDirectory(subDir);

		}
	}
	closedir(dir);
}

int receive_socket(int sock, char *rx_buffer, int rx_buffer_size, int flag) {
	int rx_length;
	char packet_length[2];

	// Receive packet length
	rx_length = recv(sock, packet_length, 2, flag);
	ESP_LOGI(__FUNCTION__, "rx_length=%d", rx_length);
	if (rx_length == 0) {
		// Connection closed by peer
		return rx_length;
	} else if (rx_length < 0) {
		ESP_LOGE(__FUNCTION__, "recv failed: errno %d", errno);
		return rx_length;
	} else if (rx_length != 2) {
		ESP_LOGE(__FUNCTION__, "illegal receive length %d", rx_length);
		return -1;
	}
	ESP_LOGD(__FUNCTION__, "packet_length=0x%x-0x%x", packet_length[0], packet_length[1]);

	// Receive packet body
	int index = 0;
	int remain_size = packet_length[0]*256+packet_length[1];
	ESP_LOGI(__FUNCTION__, "remain_size=%d", remain_size);
	while(1) {
		rx_length = recv(sock, &rx_buffer[index], remain_size, flag);
		ESP_LOGD(__FUNCTION__, "rx_length=%d", rx_length);
		if (rx_length <= 0) {
			ESP_LOGE(__FUNCTION__, "recv failed: errno %d", errno);
			return rx_length;
		}
		index = index + rx_length;
		remain_size = remain_size - rx_length;
		ESP_LOGI(__FUNCTION__, "rx_length=%d index=%d remain_size=%d", rx_length, index, remain_size);
		if (remain_size == 0) break;
	}
	return index;
}

int send_ok(int sock) {
	char tx_buffer[2];
	memcpy(tx_buffer, "OK", 2);
	int err = send(sock, tx_buffer, 2, 0);
	if (err < 0) {
		ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
	}
	return err;
}

int send_ng(int sock) {
	char tx_buffer[2];
	memcpy(tx_buffer, "NG", 2);
	int err = send(sock, tx_buffer, 2, 0);
	if (err < 0) {
		ESP_LOGE(__FUNCTION__, "Error occurred during sending: errno %d", errno);
	}
	return err;
}

// File copy from Host to SPIFFS
void put_file(int sock, char *filepath) {
	char rx_buffer[RX_BUFFER_SIZE+1];
	char md5[33];
	FILE *file = NULL;
	int fileOpen = 0;
	int fileSize = 0;
	struct MD5Context context;
	int tx_length;

	while (1) {
		//int rx_length = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		int rx_length = receive_socket(sock, rx_buffer, RX_BUFFER_SIZE, 0);
		// Error occurred during receiving
		if (rx_length <= 0) {
			ESP_LOGE(__FUNCTION__, "receive_socket failed: %d", rx_length);
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
				tx_length = send_ng(sock);
				if (tx_length < 0) break;
			} else {
				fileOpen = 1;
				fileSize = 0;
				esp_rom_md5_init(&context);
				tx_length = send_ok(sock);
				if (tx_length < 0) break;
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
					tx_length = send_ok(sock);
					if (tx_length < 0) break;
				} else {
					ESP_LOGE(__FUNCTION__, "File copied NG - invalid hash");
					tx_length = send_ng(sock);
					if (tx_length < 0) break;
				}
				file = NULL;
				fileOpen = 0;
			} else {
				tx_length = send_ng(sock);
				if (tx_length < 0) break;
			}
			break;
		} // end tailer


		else {
			if (fileOpen) {
				// Write file
				int ret = fwrite(rx_buffer, rx_length, 1, file);
				if (ret != 1) {
					ESP_LOGE(__FUNCTION__, "Failed to write file");
					tx_length = send_ng(sock);
					if (tx_length < 0) break;
				} else {
					// Update md5
					fileSize = fileSize + rx_length;
					esp_rom_md5_update(&context, rx_buffer, rx_length);
					tx_length = send_ok(sock);
					if (tx_length < 0) break;
				}
			}
		} // end data

	} // end while
}

int send_packet(int sock, char *tx_buffer, int tx_length) {
	ESP_LOGI(__FUNCTION__, "tx_length=%d", tx_length);
	char packet_length[2];
	packet_length[0] = tx_length >> 8;
	packet_length[1] = tx_length & 0xff;
	ESP_LOGI(__FUNCTION__, "packet_length=0x%x-0x%x", packet_length[0], packet_length[1]);
	int err = send(sock, packet_length, 2, 0);
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
	//int rx_length = receive_socket(sock, rx_buffer, 2, 0);
	int rx_length = recv(sock, rx_buffer, 2, 0);
	ESP_LOGD(__FUNCTION__, "rx_length=%d", rx_length);
	// Error occurred during receiving
	if (rx_length != 2) {
		ESP_LOGE(__FUNCTION__, "receive_socket failed: %d", rx_length);
		return rx_length;
	}
	return tx_length;
}

int send_header(int sock, int filesize)
{
	char tx_buffer[BLOCK_SIZE];
	memset(tx_buffer, 0x2C, BLOCK_SIZE); // fill canma
	memcpy(&tx_buffer[0], "header", 6);
	char filesizeChar[16];
	int filesizeLen = sprintf(filesizeChar, "%d", filesize);
	memcpy(&tx_buffer[8], filesizeChar, filesizeLen);

	int ret = send_packet(sock, tx_buffer, BLOCK_SIZE);
	ESP_LOGI(__FUNCTION__, "send_packet ret=%d", ret);
	return ret;
}

int send_tailer(int sock, char *md5)
{
	char tx_buffer[BLOCK_SIZE];
	memset(tx_buffer, 0x2C, BLOCK_SIZE); // fill canma
	memcpy(&tx_buffer[0], "tailer", 6);
	memcpy(&tx_buffer[8], md5, strlen(md5));

	int ret = send_packet(sock, tx_buffer, BLOCK_SIZE);
	ESP_LOGI(__FUNCTION__, "send_packet ret=%d", ret);
	return ret;
}

// File copy from SPIFFS to Host
void get_file(int sock, char *filepath) {
	int filesize = getFileSize(filepath);
	int ret = send_header(sock, filesize);
	ESP_LOGI(__FUNCTION__, "send_header ret=%d", ret);
	if (ret <= 0) {
		ESP_LOGE(__FUNCTION__, "send_header fail %d", ret);
		return;
	}
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
		int ret = fread(tx_buffer, 1, RX_BUFFER_SIZE, file);
		ESP_LOGI(TAG, "fread rer=%d", ret);
		if (ret == 0) break;
		ret = send_packet(sock, tx_buffer, ret);
		if (ret <= 0) {
			ESP_LOGE(__FUNCTION__, "send_packet fail %d", ret);
			return;
		}
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
	ret = send_tailer(sock, hexdigest);
	if (ret <= 0) {
		ESP_LOGE(__FUNCTION__, "send_tailer fail %d", ret);
		return;
	}
}

// Delete file from SPIFFS
void del_file(int sock, char *filepath) {
	ESP_LOGI(__FUNCTION__, "unlink [%s]", filepath);
	unlink(filepath);
}

void comp_task(void *pvParameters);

void receive_command(struct sockaddr_in6 source_addr, int sock, char *mount_point) {
	char rx_buffer[RX_BUFFER_SIZE+1];
	char filepath[FILE_PATH_MAX];
	char addr_str[128];
	int tx_length;
	struct stat st;

	while (1) {
		//int rx_length = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		int rx_length = receive_socket(sock, rx_buffer, RX_BUFFER_SIZE, 0);
		ESP_LOGI(__FUNCTION__, "rx_length=%d", rx_length);
		// Error occurred during receiving
		if (rx_length == 0) {
			ESP_LOGI(__FUNCTION__, "Connection closed");
			break;
		} else if (rx_length < 0) {
			ESP_LOGE(__FUNCTION__, "receive_socket failed: %d", rx_length);
			break;
		}
		// Receive payload
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

		char *adr = strchr(rx_buffer, (int)',');
		ESP_LOGI(__FUNCTION__, "filename=[%s]", adr+1);
		memset(filepath, 0, sizeof(filepath));
		strcpy(filepath, mount_point);
		strcat(filepath, "/");
		strcat(filepath, adr+1);
		ESP_LOGI(__FUNCTION__, "filepath=[%s]", filepath);

		// Receive file from Host
		if (strncmp(rx_buffer, "put_file", 8) == 0) {
			tx_length = send_ok(sock);
			if (tx_length < 0) break;
			put_file(sock, filepath);
			printDirectory(mount_point);

		// Send file to Host
		} else if (strncmp(rx_buffer, "get_file", 8) == 0) {
			if (stat(filepath, &st) == 0) {
				tx_length = send_ok(sock);
				if (tx_length < 0) break;
				get_file(sock, filepath);
			} else {
				ESP_LOGE(__FUNCTION__, "file not found [%s]", filepath);
				tx_length = send_ng(sock);
				if (tx_length < 0) break;
			}

		// Delete file from SPIFFS
		} else if (strncmp(rx_buffer, "del_file", 8) == 0) {
			if (stat(filepath, &st) == 0) {
				tx_length = send_ok(sock);
				if (tx_length < 0) break;
				printDirectory(mount_point);
				del_file(sock, filepath);
				printDirectory(mount_point);
			} else {
				ESP_LOGE(__FUNCTION__, "file not found [%s]", filepath);
				tx_length = send_ng(sock);
				if (tx_length < 0) break;
			}

		// Compress file on SPIFFS
		} else if (strncmp(rx_buffer, "compress", 8) == 0) {
			if (stat(filepath, &st) == 0) {
				tx_length = send_ok(sock);
				if (tx_length < 0) break;
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
				if (comp_result != 0) break;
				printDirectory(mount_point);
			} else {
				ESP_LOGE(__FUNCTION__, "file not found [%s]", filepath);
				tx_length = send_ng(sock);
				if (tx_length < 0) break;
			}

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
