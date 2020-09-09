/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file	libe-kermit.c
 * @brief	e-kermit library
 * @author	ODAV
 * @date	2017-03-13
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef X_OK
#undef X_OK
#endif /* X_OK */

#include "cdefs.h"
#include "debug.h"
#include "kermit.h"
#include "libe-kermit.h"
#include "unixio.h"

#include <string.h>

#ifndef BSP_INTERNAL_BUFFER_SIZE
uint8_t o_buf[OBUFLEN + 8] = {0}; /* File output buffer */
uint8_t i_buf[IBUFLEN + 8] = {0}; /* File input buffer */
struct k_data k = {0};						/* Kermit data structure */
struct k_response r = {0};				/* Kermit response structure */
#else
extern uint8_t *_bsp_internal_buffer;
uint8_t *o_buf = NULL;			 /* File output buffer */
uint8_t *i_buf = NULL;			 /* File input buffer */
struct k_data *k = NULL;		 /* Kermit data structure */
struct k_response *r = NULL; /* Kermit response structure */
#endif

#ifdef DEBUG
unsigned int errorrate = 0;
int xerror()
{
	unsigned int x = 0;
	if (!errorrate)
		return (0);
	x = rand() % 100; /* Fix this - NO C LIBRARY */
	debug(DB_LOG, "RANDOM", 0, x);
	debug(DB_LOG, "ERROR", 0, (x < errorrate));
	return (x < errorrate);
}
#endif /* DEBUG */

static int kermit_main(int action, UCHAR **cmlist, UCHAR **arg, UCHAR *type, int *nresend)
{
#ifndef BSP_INTERNAL_BUFFER_SIZE
	int status = 0, rx_len = 0, retrycounter = 0;
	int start = 1;
	int ret = K_SUCCESS;
	UCHAR *inbuf = 0;

	PRINT_DDEBUG_ARG("Entering %s...\n", __FUNCTION__);
	if (kdevopen() != 0)
	{
		PRINT_DDEBUG_ARG("Entering %s...\n", __FUNCTION__);
		if (type != NULL)
			*type = A_WAIT; /* Just to put a bad return type */
		if (arg != NULL)
			*arg = NULL;
		return K_FAILURE;
	}

	r.type = A_WAIT;
	k.filelist = cmlist;

	status = kermit(K_REINIT, &k, 0, "eksw init", &r);
	if (status != X_OK)
		return K_ERROR;

	if (action == A_DIR)
	{
		status = kermit(K_DIR, &k, 0, "eksw dir", &r);
	}
	if (action == A_SEND)
	{
		status = kermit(K_SEND, &k, 0, "eksw send", &r);
	}
	if (action == A_GET)
	{
		status = kermit(K_GET, &k, 0, "eksw get", &r);
	}

	retrycounter = k.retry + 1;

	/*
	Now we read a packet ourselves and call Kermit with it.  Normally, Kermit
	would read its own packets, but in the embedded context, the device must be
	free to do other things while waiting for a packet to arrive.  So the real
	control program might dispatch to other types of tasks, of which Kermit is
	only one.  But in order to read a packet into Kermit's internal buffer, we
	have to ask for a buffer address and slot number.

	To interrupt a transfer in progress, set k.cancel to I_FILE to interrupt
	only the current file, or to I_GROUP to cancel the current file and all
	remaining files.  To cancel the whole operation in such a way that the
	both Kermits return an error status, call Kermit with K_ERROR.
	*/
	while (status != X_DONE)
	{
		/*
		Here we block waiting for a packet to come in (unless readpkt times out).
		Another possibility would be to call inchk() to see if any bytes are waiting
		to be read, and if not, go do something else for a while, then come back
		here and check again.
		*/

		if (ok2rxd(&k))
		{
			//PRINT_DDEBUG_ARG("In %s, ok2rxd...\n", __FUNCTION__);
			inbuf = k.ipktbuf;

			rx_len = k.rxd(&k, inbuf, P_PKTLEN); /* Try to read a packet */

			debug(DB_LOG, "MAIN rx_len", 0, rx_len);
			debug(DB_HEX, "MHEX", inbuf, rx_len);

			if (rx_len < 1)
			{									/* No data was read */
				if (rx_len < 0) /* If there was a fatal error */
				{
					PRINT_DDEBUG_ARG("In %s, error while reading\n", __FUNCTION__);
					ret = -rx_len;
					goto kermit_main_end;
				}
				else
				{
					PRINT_DDEBUG_ARG("In %s, timeout while reading\n", __FUNCTION__);
				}
			}
		}
		/*
		For simplicity, kermit() ACKs the packet immediately after verifying it was
		received correctly.  If, afterwards, the control program fails to handle the
		data correctly (e.g. can't open file, can't write data, can't close file),
		then it tells Kermit to send an Error packet next time through the loop.
		*/
		//PRINT_DDEBUG_ARG("In %s, before rx_len...\n", __FUNCTION__);

		/* Handle the input */
		if (rx_len == 0)
		{
			if (retrycounter == 0)
			{
				/* Handle receipt timeout situation*/
				ret = K_TIMEOUT;
				if (start == 0)
					status = kermit(K_ERROR, &k, rx_len, "eksw to", &r);
				goto kermit_main_end; // If nothing arrives at the beginning, don't send E packet, just exit now
			}
			else
			{
				retrycounter--;
				debug(DB_LOG, "kermit_main, retrycounter decreased, is now", 0, retrycounter);
			}
		}

		status = kermit(K_RUN, &k, rx_len, "", &r);

		start = 0;
		switch (status)
		{
		case X_OK:
#ifdef DEBUG
			/*
				This shows how, after each packet, you get the protocol state, file name,
				date, size, and bytes transferred so far.  These can be used in a
				file-transfer progress display, log, etc.
				*/
			debug(DB_LOG, "NAME", r.filename ? r.filename : (UCHAR *)"(NULL)", 0);
			debug(DB_LOG, "DATE", r.filedate ? r.filedate : (UCHAR *)"(NULL)", 0);
			debug(DB_LOG, "SIZE", 0, r.filesize);
			debug(DB_LOG, "STATE", 0, r.rstatus);
			debug(DB_LOG, "SOFAR", 0, r.sofar);
#endif
			/* Maybe do other brief tasks here... */
			continue; /* Keep looping */

		case X_DONE:
			debug(DB_LOG, "X_DONE exiting", 0, K_SUCCESS);
			break; /* Finished */

		case X_ERROR:
			debug(DB_LOG, "X_ERROR exiting", 0, K_FAILURE);
			ret = K_FAILURE; /* Failed */
			goto kermit_main_end;
		}
	}

kermit_main_end:
	/*  */
	debug(DB_LOG, "kermit_main_end, retrycounter", 0, retrycounter);
	debug(DB_LOG, "kermit_main_end, nresend", 0, k.nresend);
	*nresend = k.nresend;

	if (type != NULL)
		*type = r.type;
	if ((action == A_DIR) || (r.type == A_DIR))
	{
		if (arg != NULL)
			*arg = r.dir; /* We we did a dir action, save the dir content */
	}
	else
	{
		if (arg != NULL)
			*arg = r.arg;
	}
	kdevclose();
	/* Close file anyway in case of receive stop because we are not sure that the reception file is properly closed*/
	kclosefile(&k, (UCHAR)0, 2);

	return (ret);
#else // BSP_INTERNAL_BUFFER_SIZE
	int status = 0, rx_len = 0, retrycounter = 0;
	int start = 1;
	int ret = K_SUCCESS;
	UCHAR *inbuf = 0;

	PRINT_DDEBUG_ARG("Entering %s...\n", __FUNCTION__);
	if (kdevopen() != 0)
	{
		PRINT_DDEBUG_ARG("Entering %s...\n", __FUNCTION__);
		if (type != NULL)
			*type = A_WAIT; /* Just to put a bad return type */
		if (arg != NULL)
			*arg = NULL;
		return K_FAILURE;
	}

	r->type = A_WAIT;
	k->filelist = cmlist;

	status = kermit(K_REINIT, k, 0, "eksw init", r);
	if (status != X_OK)
		return K_ERROR;

	if (action == A_DIR)
	{
		status = kermit(K_DIR, k, 0, "eksw dir", r);
	}
	if (action == A_SEND)
	{
		status = kermit(K_SEND, k, 0, "eksw send", r);
	}
	if (action == A_GET)
	{
		status = kermit(K_GET, k, 0, "eksw get", r);
	}

	retrycounter = k->retry + 1;

	/*
	Now we read a packet ourselves and call Kermit with it.  Normally, Kermit
	would read its own packets, but in the embedded context, the device must be
	free to do other things while waiting for a packet to arrive.  So the real
	control program might dispatch to other types of tasks, of which Kermit is
	only one.  But in order to read a packet into Kermit's internal buffer, we
	have to ask for a buffer address and slot number.

	To interrupt a transfer in progress, set k->cancel to I_FILE to interrupt
	only the current file, or to I_GROUP to cancel the current file and all
	remaining files.  To cancel the whole operation in such a way that the
	both Kermits return an error status, call Kermit with K_ERROR.
	*/
	while (status != X_DONE)
	{
		/*
		Here we block waiting for a packet to come in (unless readpkt times out).
		Another possibility would be to call inchk() to see if any bytes are waiting
		to be read, and if not, go do something else for a while, then come back
		here and check again.
		*/

		if (ok2rxd(k))
		{
			PRINT_DDEBUG_ARG("In %s, ok2rxd...\n", __FUNCTION__);
			inbuf = k->ipktbuf;

			rx_len = k->rxd(k, inbuf, P_PKTLEN); /* Try to read a packet */

			debug(DB_LOG, "MAIN rx_len", 0, rx_len);
			debug(DB_HEX, "MHEX", inbuf, rx_len);

			if (rx_len < 1)
			{									/* No data was read */
				if (rx_len < 0) /* If there was a fatal error */
				{
					PRINT_DDEBUG_ARG("In %s, error while reading\n", __FUNCTION__);
					ret = -rx_len;
					goto kermit_main_end;
				}
				else
				{
					PRINT_DDEBUG_ARG("In %s, timeout while reading\n", __FUNCTION__);
				}
			}
		}
		/*
		For simplicity, kermit() ACKs the packet immediately after verifying it was
		received correctly.  If, afterwards, the control program fails to handle the
		data correctly (e.g. can't open file, can't write data, can't close file),
		then it tells Kermit to send an Error packet next time through the loop.
		*/
		PRINT_DDEBUG_ARG("In %s, before rx_len...\n", __FUNCTION__);

		/* Handle the input */
		if (rx_len == 0)
		{
			if (retrycounter == 0)
			{
				/* Handle receipt timeout situation*/
				ret = K_TIMEOUT;
				if (start == 0)
					status = kermit(K_ERROR, k, rx_len, "eksw to", r);
				goto kermit_main_end; // If nothing arrives at the beginning, don't send E packet, just exit now
			}
			else
			{
				retrycounter--;
			}
		}

		status = kermit(K_RUN, k, rx_len, "", r);

		start = 0;
		switch (status)
		{
		case X_OK:
#ifdef DEBUG
			/*
				This shows how, after each packet, you get the protocol state, file name,
				date, size, and bytes transferred so far.  These can be used in a
				file-transfer progress display, log, etc.
				*/
			debug(DB_LOG, "NAME", r->filename ? r->filename : (UCHAR *)"(NULL)", 0);
			debug(DB_LOG, "DATE", r->filedate ? r->filedate : (UCHAR *)"(NULL)", 0);
			debug(DB_LOG, "SIZE", 0, r->filesize);
			debug(DB_LOG, "STATE", 0, r->rstatus);
			debug(DB_LOG, "SOFAR", 0, r->sofar);
#endif
			/* Maybe do other brief tasks here... */
			continue; /* Keep looping */

		case X_DONE:
			debug(DB_LOG, "X_DONE exiting", 0, K_SUCCESS);
			break; /* Finished */

		case X_ERROR:
			debug(DB_LOG, "X_ERROR exiting", 0, K_FAILURE);
			ret = K_FAILURE; /* Failed */
			goto kermit_main_end;
		}
	}

kermit_main_end:
	/*  */
	if (type != NULL)
		*type = r->type;
	if ((action == A_DIR) || (r->type == A_DIR))
	{
		if (arg != NULL)
			*arg = r->dir; /* We we did a dir action, save the dir content */
	}
	else
	{
		if (arg != NULL)
			*arg = r->arg;
	}
	kdevclose();
	/* Close file anyway in case of receive stop because we are not sure that the reception file is properly closed*/
	kclosefile(k, (UCHAR)0, 2);

	return (ret);
#endif // BSP_INTERNAL_BUFFER_SIZE
}

/*-----------------------------------------------------------------------------
 * _EK_init()
 *-----------------------------------------------------------------------------*/
int _EK_init(char *root_path, void *priv)
{
#ifndef BSP_INTERNAL_BUFFER_SIZE
	int status = X_OK;
#else
	unsigned int tmp32 = 0;
#endif

	if (root_path == NULL)
		return (K_FAILURE);

	if (priv == NULL)
		return (K_FAILURE);

	debug(DB_MSG, "==========", 0, 0);
	debug(DB_OPN, "debug.log", 0, 0);
	debug(DB_MSG, "Initializing...", 0, 0);

#ifndef BSP_INTERNAL_BUFFER_SIZE
	/*  Fill in parameters for this run */
	memset(&k, 0, sizeof(k));

	k.wslots_max = 1; // default to use one window slot
	k.p_maxlen = P_PKTLEN;
	k.send_pause_us = 1000000;
	k.baud = 115200;

	k.remote = 0;				 /* 0 = local, 1 = remote */
	k.xfermode = 0;			 /* 0 = automatic, 1 = manual */
	k.binary = BINARY;	 /* 0 = text, 1 = binary */
	k.parity = P_PARITY; /* Default parity = PAR_NONE */
	k.bct = 1;					 /* Block check type */
	k.bcta3 = 0;

	k.ikeep = 0; /* Keep incompletely received files */
	k.cancel = 0;

	/*  Fill in the i/o pointers  */
	k.zinbuf = i_buf;		 /* File input buffer */
	k.zinlen = IBUFLEN;	 /* File input buffer length */
	k.zincnt = 0;				 /* File input buffer position */
	k.obuf = o_buf;			 /* File output buffer */
	k.obuflen = OBUFLEN; /* File output buffer length */
	k.filelist = NULL;	 /*Send file to null*/
	strncpy((char *)k.rootpath, (char *)root_path, K_ROOTPATH_LEN);

	/* Fill in function pointers */

	k.rxd = kreadpkt;		 /* for reading packets */
	k.txd = ktx_data;		 /* for sending packets */
	k.openf = kopenfile; /* for opening files */
	k.readf = kreadfile; /* for opening files */
	k.finfo = kfileinfo;
	k.writef = kwritefile; /* for writing to output file */
	k.closef = kclosefile; /* for closing files */
#ifdef DEBUG
	k.dbf = dodebug; /* for debugging */
#endif
	k.getdirdata = kgetdirdata;
	k.accessf = kaccessfile;
	k.priv = priv;

	/* Initialize Kermit protocol */
	status = kermit(K_INIT, &k, 0, "eksw init", &r);
#ifdef DEBUG
	debug(DB_LOG, "init status:", 0, status);
	debug(DB_LOG, "version:", k.version, 0);
#endif
	if (status == X_ERROR)
	{
		return (K_FAILURE);
	}
#else // BSP_INTERNAL_BUFFER_SIZE
	tmp32 = (uint32_t)(((sizeof(struct k_data) + 3) / 4) * 4 + ((sizeof(struct k_response) + 3) / 4) * 4 + OBUFLEN + IBUFLEN + 16);
	if (tmp32 > (BSP_INTERNAL_BUFFER_SIZE - 1536))
		return (K_FAILURE);

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))
	BUILD_BUG_ON(sizeof(struct k_response) != 356);
	BUILD_BUG_ON(sizeof(struct k_data) != 4744);
#if ((IBUFLEN != 512) || (OBUFLEN != 512))
#error You have to change the memory mapping of the other packages!!!
#endif
	o_buf = (uint8_t *)(&_bsp_internal_buffer + 1536 / 4);
	i_buf = (uint8_t *)(&_bsp_internal_buffer + 1536 / 4 + OBUFLEN / 4 + 8 / 4);
	r = (struct k_response *)(&_bsp_internal_buffer + 1536 / 4 + OBUFLEN / 4 + 8 / 4 + IBUFLEN / 4 + 8 / 4);
	// It is important to put k at the last position
	k = (struct k_data *)(&_bsp_internal_buffer + 1536 / 4 + OBUFLEN / 4 + 8 / 4 + IBUFLEN / 4 + 8 / 4 + ((sizeof(struct k_response) + 3) / 4));
#endif

	if (kdevinit() != 0)
	{
		return (K_FAILURE);
	}

	return K_SUCCESS;
}

/*-----------------------------------------------------------------------------
 * _EK_deinit()
 *-----------------------------------------------------------------------------*/
int _EK_deinit(void)
{
	/* You should have check that init has been done before... */
	int err = kdevdeinit();
	return (err);
}

/*-----------------------------------------------------------------------------
 * _EK_init_memory()
 *-----------------------------------------------------------------------------*/
int _EK_init_memory(char *root_path, void *priv)
{
#ifndef BSP_INTERNAL_BUFFER_SIZE
	return K_SUCCESS;
#else
	int status = X_OK;

	/*  Fill in parameters for this run */
	memset(k, 0, sizeof(struct k_data));

	k->wslots_max = 1; // default to use one window slot
	k->p_maxlen = P_PKTLEN;
	k->send_pause_us = 1000000;
	k->baud = 115200;

	k->remote = 0;				/* 0 = local, 1 = remote */
	k->xfermode = 0;			/* 0 = automatic, 1 = manual */
	k->binary = BINARY;		/* 0 = text, 1 = binary */
	k->parity = P_PARITY; /* Default parity = PAR_NONE */
	k->bct = 1;						/* Block check type */
	k->bcta3 = 0;

	k->ikeep = 0; /* Keep incompletely received files */
	k->cancel = 0;

	/*  Fill in the i/o pointers  */
	k->zinbuf = i_buf;		/* File input buffer */
	k->zinlen = IBUFLEN;	/* File input buffer length */
	k->zincnt = 0;				/* File input buffer position */
	k->obuf = o_buf;			/* File output buffer */
	k->obuflen = OBUFLEN; /* File output buffer length */
	k->filelist = NULL;		/*Send file to null*/
	strncpy((char *)k->rootpath, (char *)root_path, K_ROOTPATH_LEN);

	/* Fill in function pointers */

	k->rxd = kreadpkt;		/* for reading packets */
	k->txd = ktx_data;		/* for sending packets */
	k->openf = kopenfile; /* for opening files */
	k->readf = kreadfile; /* for opening files */
	k->finfo = kfileinfo;
	k->writef = kwritefile; /* for writing to output file */
	k->closef = kclosefile; /* for closing files */
#ifdef DEBUG
	k->dbf = dodebug;				/* for debugging */
#endif
	k->getdirdata = kgetdirdata;
	k->accessf = kaccessfile;
	k->priv = priv;

	/* Initialize Kermit protocol */
	status = kermit(K_INIT, k, 0, "eksw init", r);
#ifdef DEBUG
	debug(DB_LOG, "init status:", 0, status);
	debug(DB_LOG, "version:", k->version, 0);
#endif
	if (status == X_ERROR)
		return (K_FAILURE);
	else
		return K_SUCCESS;
#endif
}

/*-----------------------------------------------------------------------------
 * _EK_start_server()
 *-----------------------------------------------------------------------------*/
uint8_t _EK_start_server(ek_transaction_type_e *type, char **arg, int *nresend)
{
	int ret = K_SUCCESS;
	unsigned char k_type = 0;

	ret = kermit_main(A_WAIT, NULL, (UCHAR **)arg, &k_type, nresend);

	/* Translate type */
	switch (k_type)
	{
	case A_GET:
		*type = ek_type_get;
		break;
	case A_SEND:
		*type = ek_type_send;
		break;
	case A_DIR:
		*type = ek_type_dir;
		break;
	default:
		*type = ek_type_other;
		break;
	}

	return ret;
}

/*-----------------------------------------------------------------------------
 * _EK_get()
 *-----------------------------------------------------------------------------*/
int _EK_get(char *filename)
{
	int ret = K_SUCCESS;
	unsigned char **filelist = (unsigned char **)0; /* Pointer to file list */
	unsigned char *array[2] = {0};

	/* You should have check first that filename is not NULL, and init has been done before... */

	array[0] = (UCHAR *)filename;
	array[1] = (unsigned char *)0;
	filelist = array;

	ret = kermit_main(A_GET, (UCHAR **)filelist, NULL, NULL, NULL);

	return ret;
}

/*-----------------------------------------------------------------------------
 * _EK_send()
 *-----------------------------------------------------------------------------*/
int _EK_send(char *filename)
{
	int ret = K_SUCCESS;
	unsigned char **filelist = (unsigned char **)0; /* Pointer to file list */
	unsigned char *array[2] = {0};

	/* You should have check first that filename is not NULL, and init has been done before... */

	array[0] = (UCHAR *)filename;
	array[1] = (unsigned char *)0;
	filelist = array;

	ret = kermit_main(A_SEND, (UCHAR **)filelist, NULL, NULL, NULL);

	return ret;
}

/*-----------------------------------------------------------------------------
 * _EK_dir()
 *-----------------------------------------------------------------------------*/
int _EK_dir(char **result)
{
	int ret = K_SUCCESS;
	unsigned char **filelist = (unsigned char **)0; /* Pointer to file list */
	unsigned char *array[2] = {0};

	/* You should have check first that result is not NULL, and init has been done before... */
	array[0] = (unsigned char *)0;
	array[1] = (unsigned char *)0;
	filelist = array;

	ret = kermit_main(A_DIR, (UCHAR **)filelist, (UCHAR **)result, NULL, NULL);

	return ret;
}
