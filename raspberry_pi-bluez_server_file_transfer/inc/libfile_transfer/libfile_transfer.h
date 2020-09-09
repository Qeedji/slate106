/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file	libfile_transfer.h
 * @brief	header of the library
 * @author	SROG
 * @date	2017-03-03
 */

#ifndef INC_LIBFILE_TRANSFER_H_
#define INC_LIBFILE_TRANSFER_H_

/*=============================================================================
 * include
 *=============================================================================*/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*=============================================================================
 * define
 *=============================================================================*/

#define FT_SUCCESS (0)
#define FT_ETIME (62)	 /* Timer expired */
#define FT_EIO (5)		 /* I/O error */
#define FT_EACCES (13) /* Permission denied */

#define FT_EINVAL (22) /* Invalid argument */
#define FT_ECONNRESET (54)

/*=============================================================================
 * enum
 *=============================================================================*/
typedef enum
{
	ft_mode_server,
	ft_mode_client
} ft_mode_e;

typedef enum
{
	ft_type_get,
	ft_type_send,
	ft_type_dir,
	ft_type_end,
	ft_type_other
} ft_transaction_type_e;

/*=============================================================================
 * function
 *=============================================================================*/
unsigned char FT_init(ft_mode_e mode, const char *root_path, void *priv);
unsigned char FT_deinit(void);
unsigned char FT_start_server(ft_transaction_type_e *type, char **arg, int *nresend);
unsigned char FT_init_memory(void);
unsigned char FT_get(char *filename);
unsigned char FT_send(char *filename);
unsigned char FT_dir(char **result);

#endif /* INC_LIBFILE_TRANSFER_H_ */
