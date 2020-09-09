#ifndef H_FIFO
#define H_FIFO

#include <stdint.h>

#define MLDP_RX_BUFF_SIZE 4096
#define MLDP_TX_BUFF_SIZE 4096

typedef struct
{
  uint8_t *p_buff;
  uint32_t buff_size;
  uint32_t read_pos;
  uint32_t write_pos;
  uint8_t fifo[MLDP_RX_BUFF_SIZE];
} fifo;

int fifo_init(fifo *p_fifo, uint8_t *p_buff, uint16_t buf_size);
int fifo_put(fifo *p_fifo, uint8_t byte);
int fifo_get(fifo *p_fifo, uint8_t *byte);
int fifo_read(fifo *p_fifo, uint8_t *p_byte_array, uint32_t *p_size);
int fifo_write(fifo *p_fifo, const uint8_t *p_byte_array, uint32_t *p_size);
int is_fifo_empty(fifo *p_fifo);
int is_fifo_full(fifo *p_fifo);
void fifo_display(fifo *p_fifo);
int fifo_flush(fifo *p_fifo);
size_t fifo_length(fifo *p_fifo);

#endif
