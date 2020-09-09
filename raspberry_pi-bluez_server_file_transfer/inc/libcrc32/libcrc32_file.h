/**
* Copyright (c) 2016, Innes SA,
* All Rights Reserved
*
* The copyright notice above does not evidence any
* actual or intended publication of such source code.
*/

/**
 * @file	libcrc32_file.h
 * @brief	crc32_file library include
 * @author	O. DAVID
 * @date	2016-10-21
 *       
 *
 */

#ifndef LIBCRC32_FILE_H
#define LIBCRC32_FILE_H

/**
 *  @brief values of CRCF_FILE_SYSTEM_API
 */
#define CRCF_LIBC 		0
#define CRCF_FATFS 		1
#define CRCF_PFATFS		2

/**
 *  @brief values of CRCF_CRC_ALGORITHM
 */
#define CRCF_SOFT		0
#define CRCF_STM32_HAL	1

#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL)
	#include "stm32l1xx_hal.h"
#endif

#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
	#include "ff.h"
	#include "flash.h"
#elif CRCF_FILE_SYSTEM_API == CRCF_PFATFS
	#include "pff.h"
#elif CRCF_FILE_SYSTEM_API == CRCF_LIBC
	#include <stdint.h>
	#include <stdlib.h>
	#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
	#if ( (_MAX_SS != 4096) && (_MIN_SS != 4096) )
		#error In ffconf.h(fatfs), the sector size must be =4096 !!!!!
	#endif
	#define SECTOR_SIZE_BYTES			4096
	#define SECTOR_SIZE_LOG				12
#endif

#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
	typedef FIL CRCF_FILE;
#elif CRCF_FILE_SYSTEM_API == CRCF_PFATFS
	typedef UINT CRCF_FILE;
#elif CRCF_FILE_SYSTEM_API == CRCF_LIBC
	typedef FILE CRCF_FILE;
#endif


/**
 * @brief 			calculate the CRC32 of a file
 * @param file 		File pointer (FIL* for fatfs, int* for libc which points to file descriptor)
 * @param start		start offset in bytes. NULL to start to the beginning of the file.
 * @param length	length of bytes on which applying the crc calculation. NULL will take (file_length - start), without the need to calculate it
 * @param crc		crc calculated.
 * @return 			Positive value indicate error on errno standard
 * 					0 value indicate the crc is ok
 */
uint8_t CRCF_calc_crc(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc);

#ifdef __cplusplus
}
#endif

#endif /* LIBCRC32_FILE_H */
