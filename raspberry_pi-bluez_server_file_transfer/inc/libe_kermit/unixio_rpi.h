#ifndef UNIXIO_H
#define UNIXIO_H

#include <stdint.h>
#include "fifo.h"

typedef struct unixio_rpi
{
  int (*ble_mldp_send_bytes)(const uint8_t *p_string, uint32_t length);
  int (*ble_mldp_get_byte)(uint8_t *p_byte);
  void (*ft_wait_ms)(uint32_t timeMS);
} unixio_rpi_t;

#endif
