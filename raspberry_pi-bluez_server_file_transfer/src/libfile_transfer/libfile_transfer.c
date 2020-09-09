/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file	libfile_transfer.c
 * @brief	file transfer library
 * @author	SROG
 * @date	2017-03-03
 */

/*=============================================================================
 * include
 *=============================================================================*/
#include "libfile_transfer.h"
#include "libe-kermit.h"
#include "cdefs.h"
#include "kermit.h"
#include <string.h>

/*=============================================================================
 * define
 *=============================================================================*/

#define FT_IS_NOT_INITIALIZED (0)
#define FT_IS_INITIALIZED (1)

/*=============================================================================
 * internal structures
 *=============================================================================*/
typedef struct _FT_sStatus
{
	uint8_t state;
	ft_mode_e mode;
} FT_sStatus;

static FT_sStatus gStatus = {
		.state = FT_IS_NOT_INITIALIZED,
		.mode = ft_mode_client};

static char *ft_rp = NULL;
static void *ft_priv = NULL;

/*-----------------------------------------------------------------------------
 * 													FT_init()
 *-----------------------------------------------------------------------------*/
unsigned char FT_init(ft_mode_e mode, const char *root_path, void *priv)
{
	unsigned char ret = FT_SUCCESS;

	if (gStatus.state == FT_IS_INITIALIZED)
		return EACCES;

	if (root_path == NULL)
		return EINVAL;

	if (priv == NULL)
		return EINVAL;

	ft_rp = (char *)root_path;
	ft_priv = priv;
	ret = (unsigned char)_EK_init((char *)root_path, priv);

	gStatus.mode = mode;
	gStatus.state = FT_IS_INITIALIZED;

	switch (ret)
	{
	case K_SUCCESS:
		return FT_SUCCESS;
	case K_TIMEOUT:
		return ETIME;
	default:
		return EIO;
	}
}

/*-----------------------------------------------------------------------------
 * 													FT_deinit()
 *-----------------------------------------------------------------------------*/
unsigned char FT_deinit(void)
{
	unsigned char ret = FT_SUCCESS;

	if (gStatus.state == FT_IS_NOT_INITIALIZED)
	{
		return EACCES;
	}
	ret = (unsigned char)_EK_deinit();
	gStatus.state = FT_IS_NOT_INITIALIZED; /* Library considered as freed, even if there are errors */

	if (ret)
	{
		return EIO;
	}
	else
	{
		return FT_SUCCESS;
	}
}

/*-----------------------------------------------------------------------------
 * 													FT_start_server()
 *-----------------------------------------------------------------------------*/
unsigned char FT_start_server(ft_transaction_type_e *type, char **arg, int *nresend)
{
	unsigned char ret = FT_SUCCESS;
	ek_transaction_type_e ek_type = ft_type_other;

	if ((gStatus.state == FT_IS_NOT_INITIALIZED) || (gStatus.mode != ft_mode_server))
		return EACCES;

	if ((type == NULL) || (arg == NULL))
		return EINVAL;

	ret = (unsigned char)_EK_start_server(&ek_type, arg, nresend);

	/* Translate type */
	switch (ek_type)
	{
	case ek_type_get:
		*type = ft_type_get;
		break;
	case ek_type_send:
		*type = ft_type_send;
		break;
	case ek_type_dir:
		*type = ft_type_dir;
		break;
	default:
		*type = ft_type_other;
		break;
	}

	switch (ret)
	{
	case K_SUCCESS:
		return FT_SUCCESS;
	case K_TIMEOUT:
		return ETIME;
	case K_END:
		*type = ft_type_end;
		return FT_SUCCESS;
	default:
		return EIO;
	}
}

/*-----------------------------------------------------------------------------
 * 													FT_init_memory()
 *-----------------------------------------------------------------------------*/
unsigned char FT_init_memory(void)
{
	if (_EK_init_memory(ft_rp, ft_priv) == K_SUCCESS)
		return FT_SUCCESS;
	else
		return EIO;
}

/*-----------------------------------------------------------------------------
 * 													FT_get()
 *-----------------------------------------------------------------------------*/
unsigned char FT_get(char *filename)
{
	unsigned char ret = FT_SUCCESS;

	if ((gStatus.state == FT_IS_NOT_INITIALIZED) || (gStatus.mode != ft_mode_client))
		return EACCES;

	if (filename == NULL)
		return EINVAL;

	ret = (unsigned char)_EK_get(filename);

	switch (ret)
	{
	case K_SUCCESS:
		return FT_SUCCESS;
	case K_TIMEOUT:
		return ETIME;
	default:
		return EIO;
	}
}

/*-----------------------------------------------------------------------------
 * 													FT_send()
 *-----------------------------------------------------------------------------*/
unsigned char FT_send(char *filename)
{
	unsigned char ret = FT_SUCCESS;

	if ((gStatus.state == FT_IS_NOT_INITIALIZED) || (gStatus.mode != ft_mode_client))
		return EACCES;

	if (filename == NULL)
		return EINVAL;

	ret = (unsigned char)_EK_send(filename);

	switch (ret)
	{
	case K_SUCCESS:
		return FT_SUCCESS;
	case K_TIMEOUT:
		return ETIME;
	default:
		return EIO;
	}
}

/*-----------------------------------------------------------------------------
 * 													FT_dir()
 *-----------------------------------------------------------------------------*/
unsigned char FT_dir(char **result)
{
	unsigned char ret = FT_SUCCESS;

	if ((gStatus.state == FT_IS_NOT_INITIALIZED) || (gStatus.mode != ft_mode_client))
		return EACCES;

	if (result == NULL)
		return EINVAL;

	ret = (unsigned char)_EK_dir(result);

	switch (ret)
	{
	case K_SUCCESS:
		return FT_SUCCESS;
	case K_TIMEOUT:
		return ETIME;
	default:
		return EIO;
	}
}