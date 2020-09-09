/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file   	libcrc32_file.c
 * @brief  	crc32 file library
 * @author 	O. DAVID
 * @date 	2016-10-24
 */

#include <errno.h>
#include "libcrc32_file.h"

#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL)
	//#define CRCF_TIMEOUT_MS_DATA_DMA	(10)
	extern CRC_HandleTypeDef hcrc;
	#if (CRCF_FILE_SYSTEM_API == CRCF_FATFS)
	#define CRC_CACHE_SIZE 256
	extern uint8_t* _bsp_internal_buffer;

	extern HAL_flash_driver_t HAL_flash_driver;

	extern uint8_t _BSP_fastseek_deactivate(FIL *file);
	extern uint8_t _BSP_fastseek_activate(FIL *file);
	extern uint8_t _BSP_fastseek_addr(FIL *file, uint32_t *sect_table, uint8_t *nb_addr);
	#endif
#endif

static const uint32_t crc_table[16] =
{	// Nibble lookup table for 0x04C11DB7 polynomial
	0x00000000,0x04C11DB7,0x09823B6E,0x0D4326D9,0x130476DC,0x17C56B6B,0x1A864DB2,0x1E475005,
	0x2608EDB8,0x22C9F00F,0x2F8AD6D6,0x2B4BCB61,0x350C9B64,0x31CD86D3,0x3C8EA00A,0x384FBDBD
};

/*=============================================================================
 * WRAPPER Functions/Macros
 *=============================================================================*/
/*-----------------------------------------------------------------------------
 * fsize()
 *-----------------------------------------------------------------------------*/
#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
static inline uint32_t CRCF_fsize(CRCF_FILE* file)
{
	return (file->fsize);
}
#elif CRCF_FILE_SYSTEM_API == CRCF_PFATFS
static inline uint32_t CRCF_fsize(CRCF_FILE* file)
{
	uint32_t size=0;

	if (pf_fsize((UINT*)&size) == FR_OK)
		return size;
	else
		return 0;
}
#elif CRCF_FILE_SYSTEM_API == CRCF_LIBC
static uint32_t CRCF_fsize(CRCF_FILE* file)
{
	int pos=0;
	int end=0;

	pos = ftell(file);
	if (pos == -1)
		return 0;
	if (fseek(file, 0, SEEK_END) != 0)
		return 0;
	end = ftell(file);
	if (end == -1)
		return 0;
	if (fseek(file, pos, SEEK_SET) != 0)
	if (pos == -1)
		return 0;

	return (uint32_t)end;
}
#endif

/*-----------------------------------------------------------------------------
 * fread()
 *-----------------------------------------------------------------------------*/
#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
uint8_t CRCF_fread(CRCF_FILE* file, void* buff, uint32_t size, uint32_t* br)
{
	return f_read(file, buff, size, (UINT*)br);
}
#elif CRCF_FILE_SYSTEM_API == CRCF_PFATFS
uint8_t CRCF_fread(CRCF_FILE* file, void* buff, uint32_t size, uint32_t* br)
{
	return pf_read(buff, (UINT)size, (UINT*)br);
}
#elif CRCF_FILE_SYSTEM_API == CRCF_LIBC
#define FR_OK 0
uint8_t CRCF_fread(CRCF_FILE* file, void* buff, uint32_t size, uint32_t* br)
{
	*br = fread(buff, 1, size, file);
	return FR_OK;
}
#endif

/*-----------------------------------------------------------------------------
 * fseek()
 *-----------------------------------------------------------------------------*/
#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
uint8_t CRCF_fseek(CRCF_FILE* file, uint32_t ofs)
{
	return f_lseek(file, ofs);
}
#elif CRCF_FILE_SYSTEM_API == CRCF_PFATFS
uint8_t CRCF_fseek(CRCF_FILE* file, uint32_t ofs)
{
	return pf_lseek(ofs);
}
#elif CRCF_FILE_SYSTEM_API == CRCF_LIBC
#define FR_OK 0
uint8_t CRCF_fseek(CRCF_FILE* file, uint32_t ofs)
{
	if (fseek(file, (long)ofs, SEEK_SET) ==0)
		return FR_OK;
	else
		return 1;
}
#endif

/*-----------------------------------------------------------------------------
 * calc_crc()
 *-----------------------------------------------------------------------------*/
#if CRCF_CRC_ALGORITHM == CRCF_STM32_HAL
	#if CRCF_FILE_SYSTEM_API == CRCF_FATFS
		static uint8_t CRCF_calc_crc_soft(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc);
		static uint8_t CRCF_calc_crc_DMA(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc);
		inline uint8_t CRCF_calc_crc(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc)
		{
			return CRCF_calc_crc_DMA(file, start, length, crc);
		}
	#else
		static uint8_t CRCF_calc_crc_soft(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc);
		uint8_t CRCF_calc_crc(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc)
		{
			return CRCF_calc_crc_soft(file, start, length, crc);
		}
	#endif
#else
	static uint8_t CRCF_calc_crc_soft(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc);

	uint8_t CRCF_calc_crc(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc)
	{
		return CRCF_calc_crc_soft(file, start, length, crc);
	}
#endif

//#if CRCF_CRC_ALGORITHM == CRCF_STM32_HAL
#if 0
// This callback is called in interrupt context (strong function)
void HAL_CRC_RxCpltCallback(CRC_HandleTypeDef *hcrc)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	TaskHandle_t task;

    configASSERT( hcrc->data != NULL );
	task = (TaskHandle_t)hcrc->data;
	hcrc->data = NULL;
	/* Notify the task that the transmission is complete. */
	vTaskNotifyGiveFromISR( task, &xHigherPriorityTaskWoken );

	/* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
	should be performed to ensure the interrupt returns directly to the highest
	priority task.  The macro used for this purpose is dependent on the port in
	use and may be called portEND_SWITCHING_ISR(). */
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
#endif


#if (CRCF_CRC_ALGORITHM != CRCF_STM32_HAL)
static uint32_t Crc32Fast(uint32_t crc, uint32_t data)
{
	crc = crc ^ data; // Apply all 32-bits
	// Process 32-bits, 4 at a time, or 8 rounds
	crc = (crc << 4) ^ crc_table[crc >> 28]; // Assumes 32-bit reg, masking index to 4-bits
	crc = (crc << 4) ^ crc_table[crc >> 28]; // 0x04C11DB7 Polynomial used in STM32
	crc = (crc << 4) ^ crc_table[crc >> 28];
	crc = (crc << 4) ^ crc_table[crc >> 28];
	crc = (crc << 4) ^ crc_table[crc >> 28];
	crc = (crc << 4) ^ crc_table[crc >> 28];
	crc = (crc << 4) ^ crc_table[crc >> 28];
	crc = (crc << 4) ^ crc_table[crc >> 28];
 
	return(crc);
}
#endif

static uint8_t CRCF_calc_crc_soft(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc)
{
	uint8_t ret=0;
	uint32_t i=0,fsize=0,filret=0;
	uint32_t ffsize=0;
	uint32_t cache=0;

	ffsize = CRCF_fsize(file);

	if ( (file == NULL) || (ffsize == 0) )
		return ENOENT;


	if ( (start >= ffsize) || (length > ffsize) || ((start%4) != 0) )
		return EINVAL;

#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL)
	ret=HAL_CRC_Reset(&hcrc); // Resets the CRC without supplying any data
	if (ret != HAL_OK)
		return ret;
#else
	*crc=0xffffffff;
#endif

	ret = CRCF_fseek(file, start); // Seek to the start of datas to check
	if (ret != FR_OK)
		return ret;

	if (length != 0)
		fsize = length;
	else
		fsize = ffsize-start;

	for (i=0; i<(fsize>>2);i++)
	{
		ret = CRCF_fread(file, (void*)&cache,4,&filret); // Read in little endian
		if ( (ret != FR_OK) )
			return ret;
		if (filret != 4)
			return EIO;
#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL)
		ret=HAL_CRC_Accumulate(&hcrc, &cache, filret>>2, crc);
		if (ret != HAL_OK)
			return ret;
#else
		*crc=Crc32Fast(*crc, cache);
#endif
	}
	if ( (fsize%4) != 0 )
	{	// Last bytes
		ret = CRCF_fread(file, (void*)&cache,(fsize%4),&filret); // Read in little endian
		if ( (ret != FR_OK) )
			return ret;
		if (filret != (fsize%4))
			return EIO;
		cache = cache << (8*(4-(fsize%4))); //Shift data to the MSB
		cache |= 0xffffffff >> (8*(fsize%4)); //Fill the LSB(s) with 0xff
#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL)
		ret=HAL_CRC_Accumulate(&hcrc, &cache, 1, crc);
		if (ret != HAL_OK)
			return ret;
#else
		*crc=Crc32Fast(*crc,cache);
#endif
	}

	return 0;
}

#if (CRCF_CRC_ALGORITHM == CRCF_STM32_HAL) && (CRCF_FILE_SYSTEM_API == CRCF_FATFS)
static uint8_t CRCF_calc_crc_DMA(CRCF_FILE* file, uint32_t start, uint32_t length, uint32_t* crc)
{
	uint8_t ret=0;
	uint8_t need_fastseek=0;
	uint8_t i=0,index=0;
	uint8_t* sect_table=0;
	uint32_t tmp=0;
	uint32_t len=0,l=0,le=0;
	uint32_t fptr=0,offset=0,addr=0;
	uint8_t* cache=0;
	uint32_t* dat=0;
	uint32_t ffsize=0;
	//uint32_t ulNotificationValue;

	ffsize = CRCF_fsize(file);

	if ( (file == NULL) || (ffsize == 0) )
		return ENOENT;

	if ( (start >= ffsize) || (length > ffsize) || ((start%4) != 0) )
		return EINVAL;

	if (file->cltbl == NULL)
	{	// No fastseek done before : let's do it inside this function
		ret=_BSP_fastseek_activate(file);
		if (ret)
			return CRCF_calc_crc_soft(file, start, length, crc);
		need_fastseek = 1;
	}
	
	// Prepare dma table
	ret=_BSP_fastseek_addr(file, &tmp, &index);
	if (ret)
		goto CRCF_calc_crc_error;
	sect_table=(uint8_t*)tmp;

	// CRC calculation
	cache=(uint8_t*)(&_bsp_internal_buffer);  // Map crc datas to internal_buffer[0], max_len=256 bytes
	ret=HAL_CRC_Reset(&hcrc); // Resets the CRC without supplying any data
	if (ret != HAL_OK)
		goto CRCF_calc_crc_error;

	ret = CRCF_fseek(file, start); // Seek to the start of datas to check
	if (ret != FR_OK)
		goto CRCF_calc_crc_error;

	fptr=(uint32_t)file->fptr; // Get the start of usefull datas
	//le is the total length to compute
	if (length != 0)
		le = length;
	else
		le = ffsize-start;
	for (i=0; i<index; i++)
	{
		// Calculate the start adress of the current sector
		offset=0;
		if (i==0) // First sector
		{
			offset=fptr; // Get the start of usefull datas
			// Find the sector which corresponds to this start word.
			i = (uint8_t)(offset>>SECTOR_SIZE_LOG);
			// Recompute offset for this sector
			offset=(offset%SECTOR_SIZE_BYTES);
		}
		addr=(uint32_t)(sect_table[i]*SECTOR_SIZE_BYTES)+offset;

		// Calculate the length to read on the current sector, le-(fptr-start) is the size we have to read from here
		if ( (offset+le-(fptr-start)) < SECTOR_SIZE_BYTES )
		{
			len = le-(fptr-start);
			i = (uint8_t)(index-1); // It will be the last sector to read
		}
		else
			len = (SECTOR_SIZE_BYTES - offset);
		
		// Read on the current sector
		while(len>0)
		{
			if (len < CRC_CACHE_SIZE)
				l=len;
			else
				l=CRC_CACHE_SIZE;
			ret=HAL_flash_driver.HAL_FlashRead((uint8_t*)cache, addr, (uint16_t)l);
			if (ret != HAL_OK)
				goto CRCF_calc_crc_error;
			// CRC DMA doesn't work properly, so use HAL instead
			ret=HAL_CRC_Accumulate(&hcrc, (uint32_t *)cache, (uint16_t)(l>>2), crc);
			/*
			ret=HAL_CRC_Transmit_DMA(&hcrc, (uint8_t*)cache, (uint16_t)(l>>2));
			// Wait for the end of DMA
			ulNotificationValue = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(CRCF_TIMEOUT_MS_DATA_DMA) );
			if( ulNotificationValue != 1 )
			{	//The call to ulTaskNotifyTake() timed out.
				hcrc.data=NULL;
				ret=HAL_TIMEOUT;
				goto CRCF_calc_crc_error;
			}
			*/
			len-=l;
			if ( (l%4) != 0 )
			{	// Last bytes
				dat=(uint32_t*)(&_bsp_internal_buffer+(l/4));
				*dat = *dat << (8*(4-(l%4))); //Shift data to the MSB
				*dat |= 0xffffffff >> (8*(l%4)); //Fill the LSB(s) with 0xff
				ret=HAL_CRC_Accumulate(&hcrc, dat, 1, crc);
				if (ret != HAL_OK)
					goto CRCF_calc_crc_error;
				len=0;
			}
			addr+=CRC_CACHE_SIZE;
			fptr+=l;
		}
	}
	ret=HAL_CRC_Get(&hcrc, crc);
	if (ret != HAL_OK)
		goto CRCF_calc_crc_error;

	// Normal end
	ret=0;
	if (need_fastseek)
		ret=_BSP_fastseek_deactivate(file);
	return ret;

CRCF_calc_crc_error :
	if (need_fastseek)
		_BSP_fastseek_deactivate(file);
	return ret;
}
#endif

