/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file   	fifo.c
 * @brief  	First in first out instance
 * @author 	K. AUDIERNE
 * @date 	2020-09-10
 */

#include <stdio.h>
#include <stdlib.h>
#include "fifo.h"
#include <stdint.h>
#include <unistd.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define FIFO_ERR -1
#define FIFO_SUCCESS 0
#define FIFO_EMPTY -2
#define FIFO_FULL -3
#define FIFO_NOT_ENOUGH_BIG -4

int fifo_init(fifo *p_fifo, uint8_t *p_buff, uint16_t buf_size)
{
  if (p_buff == NULL)
    return FIFO_ERR;

  p_fifo->p_buff = p_buff;
  p_fifo->read_pos = 0;
  p_fifo->write_pos = 0;
  p_fifo->buff_size = buf_size - 1; //start at 0
  return FIFO_SUCCESS;
}

/** fifo_length -- return the number of unread element in the fifo
 * Input: p_fifo -- pointer to fifo structure
 * Return: number of unread element in the fifo
 **/
uint32_t fifo_length(fifo *p_fifo)
{
  uint32_t tmp = p_fifo->read_pos;
  return p_fifo->write_pos - tmp;
}

/** fifo_put -- put only one byte in a fifo
 * Input: p_fifo -- pointer to the fifo structure
 *        byte -- value to put in the fifo
 * Return:  FIFO8SUCCESS on success, FIFO_NOT_ENOUGH_BIG on error
 **/
int fifo_put(fifo *p_fifo, uint8_t byte)
{
  if (fifo_length(p_fifo) <= p_fifo->buff_size)
  {
    p_fifo->p_buff[p_fifo->write_pos] = byte;
    p_fifo->write_pos++;
    return FIFO_SUCCESS;
  }
  return FIFO_NOT_ENOUGH_BIG;
}

/** fifo_shift -- Shift elements of the fifo in left direction by one position
 * Input: p_fifo -- pointer to the fifo structure
 * Return:  /
 * Explanation : Used in fifo_get() --> put the following element at the entrance of the fifo
 **/
static void fifo_shift(fifo *p_fifo)
{
  int len = fifo_length(p_fifo);
  uint32_t readpos = p_fifo->read_pos;
  uint8_t tmp;
  while (readpos < len)
  {
    tmp = p_fifo->p_buff[readpos + 1];
    p_fifo->p_buff[readpos] = tmp;
    readpos++;
  }
  p_fifo->write_pos--;
}

/** fifo_get -- get only one byte from the fifo
 * Input: p_fifo -- pointer to the fifo structure
 * Output : byte -- pointer to the value extract from the fifo
 * Return:  FIFO_SUCCESS on success, FIFO_EMPTY on error
 **/
int fifo_get(fifo *p_fifo, uint8_t *p_byte)
{
  if (fifo_length(p_fifo) != 0)
  {
    *p_byte = p_fifo->p_buff[p_fifo->read_pos];
    fifo_shift(p_fifo);
    return FIFO_SUCCESS;
  }
  else
  {
    usleep(250);
    return FIFO_EMPTY;
  }
}

/** fifo_read -- get a byte array from the fifo
 * Input: p_fifo -- pointer to the fifo structure
 * Output : p_byte_array -- pointer to the bytes extract from the fifo
 *          p_size -- pointer to the number of byte extract from the fifo
 * Return:  FIFO_SUCCESS on success, FIFO_ERR on error
 **/
int fifo_read(fifo *p_fifo, uint8_t *p_byte_array, size_t *p_size)
{
  if (p_fifo == NULL || p_size == NULL)
  {
    return FIFO_ERR;
  }
  const uint32_t byte_count = fifo_length(p_fifo);
  const uint32_t requested_len = *p_size;
  uint32_t index = 0;
  uint32_t read_size = MIN(requested_len, byte_count);

  *p_size = byte_count;

  // Check if application has requested only the size.
  if (p_byte_array == NULL)
  {
    return FIFO_SUCCESS;
  }

  while (index < read_size)
  {
    fifo_get(p_fifo, &p_byte_array[index++]);
  }
  *p_size = read_size;
  return FIFO_SUCCESS;
}

/** fifo_write -- put a byte array in the fifo
 * Input: p_fifo -- pointer to the fifo structure
 *        p_byte_array -- pointer to the bytes to put in the fifo
 *        p_size -- numer of bytes to put
 * Return:  FIFO_SUCCESS on success, FIFO_ERR on error
 **/
int fifo_write(fifo *p_fifo, const uint8_t *p_byte_array, uint32_t *p_size)
{
  if (p_fifo == NULL || p_size == NULL)
  {
    return FIFO_ERR;
  }
  const uint32_t available_count = p_fifo->buff_size - fifo_length(p_fifo) + 1;
  const uint32_t requested_len = *p_size;
  uint32_t index = 0;
  uint32_t write_size = MIN(requested_len, available_count);

  (*p_size) = available_count;
  if (is_fifo_full(p_fifo))
  {
    return FIFO_FULL;
  }
  // Check if application has requested only the size.
  if (p_byte_array == NULL)
  {
    return FIFO_SUCCESS;
  }
  while (index < write_size)
  {
    fifo_put(p_fifo, p_byte_array[index]);
    index++;
  }
  (*p_size) = write_size;

  return FIFO_SUCCESS;
}

int is_fifo_full(fifo *p_fifo)
{
  return p_fifo->write_pos == sizeof(p_fifo->fifo);
}

int is_fifo_empty(fifo *p_fifo)
{
  return p_fifo->write_pos == p_fifo->read_pos;
}

void fifo_display(fifo *p_fifo)
{
  for (int i = p_fifo->write_pos - 1; i > p_fifo->read_pos; i--)
  {
    printf("%02x", p_fifo->p_buff[i]);
  }
  printf("%02x\n", p_fifo->p_buff[p_fifo->read_pos]);
}

int fifo_flush(fifo *p_fifo)
{
  p_fifo->write_pos = 0;
  p_fifo->read_pos = p_fifo->write_pos;
  return FIFO_SUCCESS;
}
