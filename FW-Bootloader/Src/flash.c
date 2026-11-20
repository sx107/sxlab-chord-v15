/*
 * flash.c
 *
 *  Created on: Sep 30, 2025
 *      Author: Sx107
 */


#include "main.h"
#include "flash.h"



_flash_res flash_wait_bsy() {
	for(uint8_t tmo = 0; tmo < 100; tmo++) {
		if((FLASH->SR & FLASH_SR_BSY) == 0) {return FL_OK;}
		_delayMs(1);
	}
	return FL_TMO;
}

_flash_res flash_wait_eop() {
	for(uint8_t tmo = 0; tmo < 50; tmo++) {
		if((FLASH->SR & FLASH_SR_EOP) == FLASH_SR_EOP) {FLASH->SR = FLASH_SR_EOP; return FL_OK;}
		_delayMs(1);
	}
	FLASH->SR = FLASH_SR_EOP;
	return FL_TMO;
}

_flash_res flash_unlock() {
	if (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
		__DSB();
		if(READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0) {
			return FL_ERR;
		}
	}
	return FL_OK;
}
_flash_res flash_lock() {
	SET_BIT(FLASH->CR, FLASH_CR_LOCK);
	return FL_OK;
}

_flash_res flash_page_erase(uint32_t pageAddress, uint32_t nPages, uint32_t* pgError) {
	if (pageAddress < FLASH_BASE || pageAddress > FLASH_BANK1_END || pageAddress >= (FLASH_BASE+FLASH_SIZE)) {return FL_BADPAR;}
	//if (pageAddress+(nPages*FLASH_PAGE_SIZE)-1 > FLASH_BANK1_END || pageAddress+(nPages*FLASH_PAGE_SIZE)-1 >= (FLASH_BASE+FLASH_SIZE)) {return FL_BADPAR;}
	RETERR(flash_wait_bsy(), FL_TMO);
	if (FLASH->SR & FLASH_SR_EOP) {
		FLASH->SR = FLASH_SR_EOP;
	}
	volatile _flash_res res;
	*pgError = 0xFFFFFFFFU;
    for(uint32_t address = pageAddress; address < ((nPages * FLASH_PAGE_SIZE) + pageAddress); address += FLASH_PAGE_SIZE)
    {
    	FLASH->CR |= FLASH_CR_PER; __DSB();
    	FLASH->AR = address; __DSB();
    	FLASH->CR |= FLASH_CR_STRT; __DSB();
    	res = flash_wait_eop();
    	FLASH->CR &= ~FLASH_CR_PER; __DMB();
    	if(res != FL_OK) {
    		*pgError = address; return FL_TMO;
    	}
    }
    return FL_OK;
}

_flash_res flash_program_32(uint32_t address, uint32_t data) {
	if (address < FLASH_BASE || address > FLASH_BANK1_END || address >= (FLASH_BASE+FLASH_SIZE)) {return FL_BADPAR;}
	RETERR(flash_wait_bsy(), FL_TMO);
	if (FLASH->SR & FLASH_SR_EOP) {
		FLASH->SR = FLASH_SR_EOP;
	}

	_flash_res res = FL_OK;

	FLASH->CR |= FLASH_CR_PG;
	*(volatile uint16_t*)(address) = (uint16_t)data;
	res = flash_wait_eop();
	if(res == FL_OK) {
		*(volatile uint16_t*)(address+2) = (uint16_t)(data >> 16);
		res = flash_wait_eop();
	}
	FLASH->CR &= ~FLASH_CR_PG;

	if(*(volatile uint32_t*)address != data) {
		return FL_VERERROR;
	}

	return res;
}
