/** \file xl3_types.h */

#ifndef __XL3_TYPES_H
#define __XL3_TYPES_H

#include "main.h"

typedef struct {
    uint16_t packet_num;
    uint8_t packet_type;
    uint8_t num_bundles;
} XL3_CommandHeader;

typedef struct {
    XL3_CommandHeader cmdHeader;
    char payload[XL3_MAX_PAYLOAD_SIZE];
} XL3_Packet;

#endif
