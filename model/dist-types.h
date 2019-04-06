#include <stdint.h>
#define MAX_PAYLOAD 65536

typedef struct payload_t {
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD];
} payload_t;

typedef struct handshake_msg_t {
    uint8_t id;
} handshake_msg_t;