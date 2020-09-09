/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file   	file_transfer_task.c
 * @brief  	File transfer thread
 * @author 	K. AUDIERNE
 * @date 	2020-09-10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_transfer_task.h"
#include <stdbool.h>
#include <signal.h>

/** file_transfer_sig_handler -- function executed by the thread created in file_transfer_start_server()
 * Input: signum -- number of the signal received
 **/
void file_transfer_sig_handler(int signum)
{
  if (signum == SIGINT)
  {
    exit(0);
  }
}

/** ft_task -- function executed by the thread created in file_transfer_start_server()
 * Input: arg -- parameter of the function
 **/
void *ft_task(void *arg)
{

  ft_t *p_ft_s = (ft_t *)arg;
  char *result = NULL;
  ft_transaction_type_e type;
  int32_t nresend = 0;
  uint32_t err_code;
  unsigned char err = 0;

  const char *root_path = "img/";

  err_code = (uint32_t)FT_init(ft_mode_server, root_path, (void *)(&(p_ft_s->kermit_handler_s)));
  if (err_code != FT_SUCCESS)
  {
    printf("file_transfer_init init err_code %u.", err_code);
  }
  int flag = 0;
  while (true)
  {
    signal(SIGINT, &file_transfer_sig_handler);
    err = FT_start_server(&type, &result, &nresend);
    switch (type)
    {
    case ft_type_get:
      flag = 1;
      printf("Client did a GET of %s", result);
      break;

    case ft_type_send:
      printf("Client did a SEND");
      break;

    case ft_type_dir:
      printf("Client did a DIR: %s.", result);
      break;

    case ft_type_end:
      printf("Client ended.");
      break;

    default:
      printf("Client did unknown request");
      break;
    }
    if (type == ft_type_end)
    {
      printf("Client ended protocol.");
    }

    if (err)
    {
      printf("Error during protocol.");
      switch (err)
      {
      case FT_EINVAL:
        printf("Invalid argument !!!");
        break;
      case FT_ETIME:
        printf("Timeout !!!");
        break;
      case FT_EIO:
        printf("Protocol error ! !");
        break;
      case FT_ECONNRESET:
        printf("Connection reset by peer !!!");
        break;
      default:
        printf("Unknown error (%u) !!!", (uint32_t)err);
        break;
      }
      break;
    }
    printf(" and it succeded after %d resend\n", nresend);
    if (flag == 1)
    {
      break;
    }
  }
  err_code = (uint32_t)FT_deinit();
  if (err_code != FT_SUCCESS)
  {
    printf("file_transfer_init deinit err_code %u.", err_code);
  }
}

/** file_transfer_start_server -- Create the file transfer thread
 * Input: p_ft_s -- file transfer structure
 **/
int file_transfer_start_server(ft_t *p_ft_s)
{
  if (pthread_create(&p_ft_s->ft_task_id, NULL, ft_task, (void *)p_ft_s) != 0)
  {
    printf("FAILED TO CREATE FT_TASK");
    return -1;
  }
  return 0;
}