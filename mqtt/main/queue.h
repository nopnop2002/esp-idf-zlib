typedef enum {REQUEST_MQTT, REQUEST_PUT, REQUEST_GET, REQUEST_LIST, REQUEST_DELETE} REQUEST;
typedef enum {RESPONCE_ACK, RESPONCE_NAK} RESPONCE;

#define RESPONCE_ACK_PAYLOAD "ACK"
#define RESPONCE_NAK_PAYLOAD "NAK"

typedef struct {
	int request;
	int responce;
	int32_t event_id;
	int payload_len;
	char payload[64];
} QUEUE_t;

