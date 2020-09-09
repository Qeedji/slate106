#ifndef FILE_TRANSFER_H__
#define FILE_TRANSFER_H__

#include <stdint.h>
#include "libfile_transfer.h"
#include "unixio_rpi.h"
#include <pthread.h>

/** ft_t -- File transfer structure
 * task_id -- identifier of the thread
 * kermit_handler_s -- Used to declare functions that make the link between Bluetooth and Kermit
 * condition -- thread condition
 * attr -- attribute of the thread condition, used to initialize the condition
 * mutex -- used to properly manage data access by each thread
 **/
typedef struct
{
  pthread_t ft_task_id;
  unixio_rpi_t kermit_handler_s;
  pthread_cond_t condition;
  pthread_condattr_t attr;
  pthread_mutex_t mutex;
} ft_t;

int file_transfer_start_server(ft_t *p_ft_s);
void *ft_task();

#endif // FILE_TRANSFER_H__
