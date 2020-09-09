/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file	unixio.h
 * @brief	header of the specific functions
 * @author	ODAV
 * @date	2017-03-14
 */

#ifndef __UNIXIO_H__
#define __UNIXIO_H__

int kaccessfile(struct k_data *k, UCHAR *s);
int kgetdirdata(struct k_data *k, UCHAR *pdf);
int kopenfile(struct k_data *k, UCHAR *s, int mode, long filesize);
ULONG kfileinfo(struct k_data *k, UCHAR *filename, UCHAR *buf, int buflen, short *type, short mode);
int kreadfile(struct k_data *k);
int kwritefile(struct k_data *k, UCHAR *s, int n);
int kclosefile(struct k_data *k, UCHAR c, int mode);
int ktx_data(struct k_data *k, UCHAR *p, int n);
int kreadpkt(struct k_data *k, UCHAR *p, int len);
int kinchk(struct k_data *k);
int kdevinit(void);
int kdevdeinit(void);
int kdevopen(void);
int kdevclose(void);

#endif /* __UNIXIO_H__ */
