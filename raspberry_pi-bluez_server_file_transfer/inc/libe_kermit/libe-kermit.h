/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file	libe-kermit.h
 * @brief	header of the library
 * @author	SROG
 * @date	2017-03-09
 */


#ifndef __LIBEKERMIT_H__
#define __LIBEKERMIT_H__

typedef enum {
	ek_type_get,
	ek_type_send,
	ek_type_dir,
	ek_type_other
} ek_transaction_type_e;

int _EK_init(char* root_path, void* priv);
int _EK_deinit(void);
uint8_t _EK_start_server(ek_transaction_type_e *type, char **arg, int *nresend);
int _EK_get(char *filename);
int _EK_send(char *filename);
int _EK_dir(char **result);
int _EK_init_memory(char *root_path, void* priv);

#endif /* __LIBEKERMIT_H__ */
