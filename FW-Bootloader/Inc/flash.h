/*
 * flash.h
 *
 *  Created on: Sep 30, 2025
 *      Author: Sx107
 */

#ifndef FLASH_H_
#define FLASH_H_

typedef enum {
	FL_OK,
	FL_ERR,
	FL_TMO,
	FL_BADPAR,
	FL_VERERROR
} _flash_res;

typedef union
{
  uint8_t buf[4];
  uint32_t value32;
} uint32x_t;               // used for flash align write

_flash_res flash_unlock();
_flash_res flash_lock();
_flash_res flash_page_erase(uint32_t pageAddress, uint32_t nPages, uint32_t* pgError);
_flash_res flash_program_32(uint32_t pageAddress, uint32_t data);

#endif /* FLASH_H_ */
