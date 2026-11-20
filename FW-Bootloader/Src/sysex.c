/*
 * sysex.c
 *
 *  Created on: Sep 29, 2025
 *      Author: Sx107
 */


#include <main.h>
#include <sysex.h>
#include <dtmf.h>
#include <flash.h>
#include <eeprom.h>
#include <clock.h>

volatile uint8_t sysex_data[1200];
volatile uint8_t parsedData[1100];
volatile uint16_t parsedDataLength = 0;
volatile uint16_t sysex_data_pos = 0;
volatile uint16_t block_reception = 0;

const uint8_t hellostring[] = "CHORD v1.5 HWv8 FW: ";
volatile uint8_t send_dtmf_data[300];
volatile uint16_t send_dtmf_data_pos = 0;
volatile uint16_t send_dtmf_data_len = 0;
volatile uint8_t send_dtmf_data_nibble = 0;

volatile sysex_mode_t sysex_mode;
volatile uint8_t last_checksum = 0;

void sysex_configure() {
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

	// First the MIDIDET
	MODIFY_REG(GPIOB->MODER, GPIO_MODER_MODER10_Msk, 0b00 << GPIO_MODER_MODER10_Pos);
	MODIFY_REG(GPIOB->PUPDR, GPIO_PUPDR_PUPDR10_Msk, 0b10 << GPIO_PUPDR_PUPDR10_Pos);


	// Configure UART
	MODIFY_REG(GPIOB->MODER, GPIO_MODER_MODER7_Msk, 0b10 << GPIO_MODER_MODER7_Pos);
	MODIFY_REG(GPIOB->AFR[0] , GPIO_AFRL_AFSEL7_Msk, 0b00 << GPIO_AFRL_AFSEL7_Pos);

	USART1->BRR = 1536; //48000000 / 31250
	MODIFY_REG(USART1->CR1, USART_CR1_M_Msk | USART_CR1_PCE_Msk | USART_CR1_TE_Msk, USART_CR1_RE);
	MODIFY_REG(USART1->CR2, USART_CR2_STOP_Msk, 0 << USART_CR2_STOP_Pos);

	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_SetPriority(USART1_IRQn, 1);

	USART1->CR1 |= USART_CR1_UE | USART_CR1_RXNEIE;
}

void parse_sysex() {
	uint16_t sysex_length = sysex_data_pos;

	uint8_t cmd1 = sysex_data[4];
	uint8_t cmd2 = sysex_data[5];

	if (cmd1 == 'R' && cmd2 == 'E') {
		// Reset state machine. Reset DTMF.
		cur_dtmf_tone = DTMF_SILENCE;
		sysex_mode = SYSEX_MODE_DEFAULT;
	} else if (cmd1 == 'H' && cmd2 == 'I') {
		// Send dial tone.
		cur_dtmf_tone = DTMF_DIALTONE;
		sysex_mode = SYSEX_MODE_DEFAULT;
	} else if (cmd1 == 'T' && cmd2 == 'T') {
		// Send test tone.
		cur_dtmf_tone = sysex_data[6] & 0b1111;
		sysex_mode = SYSEX_MODE_DEFAULT;
	} else if (cmd1 == 'U' && cmd2 == 'F') {
		// Unlock flash
		 if (flash_unlock() != FL_OK) {cur_dtmf_tone = DTMF_FLASH_ERROR; return;}
		 cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'L' && cmd2 == 'F') {
		// Lock flash
		 if (flash_lock() != FL_OK) {cur_dtmf_tone = DTMF_FLASH_ERROR; return;}
		 cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'E' && cmd2 == 'F') {
		// Erase all app flash
		cur_dtmf_tone = DTMF_WAIT;
		uint32_t err_at = 0;

		_delayMs(150);
		if (flash_page_erase(APP_ADDR, 1, &err_at) != FL_OK) {cur_dtmf_tone = DTMF_FLASH_ERROR; return;}

		for(uint32_t i = 0; i < ((APP_SIZE - 0x400) >> 10); i+=4) {
			_delayMs(150);
			if (flash_page_erase(APP_ADDR + 0x400 + (i << 10), 4, &err_at) != FL_OK) {cur_dtmf_tone = DTMF_FLASH_ERROR; return;}
		}

		cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'F' && cmd2 == 'L') {
		// Flash latest received
		if (parsedDataLength != 1029) {
			cur_dtmf_tone = DTMF_WTF;
			return;
		}
		uint32_t flAddr = (parsedData[0] << 24) | (parsedData[1] << 16) | (parsedData[2] << 8) | (parsedData[3] << 0);
		if (flAddr < APP_ADDR || flAddr > APP_ADDR + APP_SIZE) {
			cur_dtmf_tone = DTMF_FLASH_ERROR;
			return;
		}
		cur_dtmf_tone = DTMF_WAIT;
		_delayMs(150);

		for (uint16_t i = 0; i < 1024; i += 4) {

			uint32x_t content;
			content.buf[0] = parsedData[4+i];
			content.buf[1] = parsedData[5+i];
			content.buf[2] = parsedData[6+i];
			content.buf[3] = parsedData[7+i];
			_flash_res res = flash_program_32(flAddr + i, content.value32);
			if (res != FL_OK) {
				cur_dtmf_tone = DTMF_FLASH_ERROR;
				return;
			}
			if ((i & 0xFF) == 0) {_delayMs(10);}
		}

		cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'O' && cmd2 == 'K') {
		// ACK for reception.
		if (sysex_mode == SYSEX_WAIT_ACK_RESEND) {
			cur_dtmf_tone = DTMF_SILENCE;
			sysex_mode = SYSEX_WAIT_ACK;
		} else if (sysex_mode == SYSEX_WAIT_FIRST) {
			cur_dtmf_tone = DTMF_SILENCE;
			send_dtmf_data_pos = 0;
			send_dtmf_data_nibble = 0;
			sysex_mode = SYSEX_WAIT_ACK;
		} else if (sysex_mode == SYSEX_WAIT_ACK) {
			if (send_dtmf_data_pos >= send_dtmf_data_len) {
				cur_dtmf_tone = DTMF_EOF;
				sysex_mode = SYSEX_WAIT_EOF;
				return;
			}

			if (send_dtmf_data_nibble == 0) {cur_dtmf_tone = 8+(send_dtmf_data[send_dtmf_data_pos] & 0b111);}
			else if (send_dtmf_data_nibble == 1) {cur_dtmf_tone = 8+((send_dtmf_data[send_dtmf_data_pos] >> 3) & 0b111);}
			else {
				uint8_t parity = send_dtmf_data[send_dtmf_data_pos];
				parity ^= parity >> 4;
				parity ^= parity >> 2;
				parity ^= parity >> 1;
				parity &= 0b1;
				cur_dtmf_tone = 8+(((send_dtmf_data[send_dtmf_data_pos] >> 6) & 0b11) | (parity << 2));
			}

			send_dtmf_data_nibble++;
			if (send_dtmf_data_nibble > 2) {send_dtmf_data_nibble = 0; send_dtmf_data_pos++;}
		} else {
			cur_dtmf_tone = DTMF_WTF;
			sysex_mode = SYSEX_MODE_DEFAULT;
		}
	} else if (cmd1 == 'N' && cmd2 == 'O') {
		// NACK. Send this DTMF byte over, preceed with RESEND
		cur_dtmf_tone = DTMF_RESEND;
		send_dtmf_data_nibble = 0;
		if (send_dtmf_data_pos != 0) {send_dtmf_data_pos -= 1;}
		sysex_mode = SYSEX_WAIT_ACK_RESEND;
	} else if (cmd1 == 'E' && cmd2 == 'T') {
		// End DTMF transmission
		sysex_mode = SYSEX_MODE_DEFAULT;
		cur_dtmf_tone = DTMF_DIALTONE;
	} else if (cmd1 == 'I' && cmd2 == 'D') {
		// Identify myself
		send_dtmf_data_len = 0;
		for(uint8_t i = 0; i < 100; i++) {
			uint8_t b = hellostring[i];
			if (b == 0) {break;}
			send_dtmf_data[send_dtmf_data_len++] = b;
		}

		for(uint8_t i = 0; i < 100; i++) {
			uint8_t b = *((uint8_t*)(APP_ADDR + i));
			send_dtmf_data[send_dtmf_data_len++] = b;
			if (b == 0) {break;}
		}

		cur_dtmf_tone = DTMF_ACK;
		sysex_mode = SYSEX_WAIT_FIRST;
	} else if (cmd1 == 'C' && cmd2 == 'S') {
		// Send checksum: send last received checksum
		send_dtmf_data[0] = last_checksum;
		send_dtmf_data_len = 1;
		send_dtmf_data_pos = 0;
		send_dtmf_data_nibble = 0;
		cur_dtmf_tone = DTMF_ACK;
		sysex_mode = SYSEX_WAIT_FIRST;
	} else if (cmd1 == 'F' && cmd2 == 'V') {
		// Fast verify : compare 8-bit parity with received one
		uint8_t target_parity = (sysex_data[6] & 0x7F) | ((sysex_data[7] & 0b1) << 7);
		uint32_t sel_page = ((sysex_data[7] >> 1) & 0x3F) << 10;

		uint8_t parity = 0;
		for (uint32_t i = 0; i < 1024; i++) {
			parity ^= *((uint8_t*)(APP_ADDR + sel_page + i));
		}

		if (parity == target_parity) {cur_dtmf_tone = DTMF_ACK;}
		else {
			cur_dtmf_tone = DTMF_NAK;
		}
	} else if (cmd1 == 'E' && cmd2 == 'R') {
		// ER : Read eeprom
		uint32_t eeprom_addr = (((sysex_data[6] & 0x7F) << 7) | (sysex_data[7] & 0x7F)) << 4;
		if (eeprom_addr >= 256) {cur_dtmf_tone = DTMF_NAK; return;}

		eeprom_read_buffer(eeprom_addr, send_dtmf_data, 16);
		uint8_t parity = 0;
		for (uint8_t i = 0; i < 16; i++) {
			parity ^= send_dtmf_data[i];
		}
		send_dtmf_data[16] = parity;

		send_dtmf_data_len = 17;
		send_dtmf_data_pos = 0;
		send_dtmf_data_nibble = 0;
		cur_dtmf_tone = DTMF_ACK;

		sysex_mode = SYSEX_WAIT_FIRST;
	} else if (cmd1 == 'E' && cmd2 == 'W') {
		if (parsedDataLength != 259) {
			cur_dtmf_tone = DTMF_WTF;
			return;
		}

		uint32_t eepromAddr = ((parsedData[0] << 8) | (parsedData[1])) << 8;
		if (eepromAddr >= 256) {
			cur_dtmf_tone = DTMF_FLASH_ERROR;
			return;
		}

		cur_dtmf_tone = DTMF_WAIT;
		_delayMs(50);

		for (uint8_t i = 0; i < 16; i++) {
			uint8_t err = eeprom_write_buffer(eepromAddr + (i<<4), &parsedData[2]+(i << 4), 16);
			err |= eeprom_wait_available(1000);
			if (err) {cur_dtmf_tone = DTMF_FLASH_ERROR; return;}
		}

		cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'E' && cmd2 == 'V') {
		if (parsedDataLength != 259) {
			cur_dtmf_tone = DTMF_WTF;
			return;
		}

		uint32_t eepromAddr = ((parsedData[0] << 8) | (parsedData[1])) << 8;
		if (eepromAddr >= 256) {
			cur_dtmf_tone = DTMF_FLASH_ERROR;
			return;
		}

		cur_dtmf_tone = DTMF_WAIT;
		_delayMs(50);

		for (uint8_t i = 0; i < 16; i++) {
			uint8_t testBuf[16];
			eeprom_read_buffer(eepromAddr+(i << 4), testBuf, 16);
			for (uint8_t j = 0; j < 16; j++) {
				if (testBuf[j] != parsedData[2+(i<<4)+j]) {
					cur_dtmf_tone = DTMF_NAK;
					return;
				}
			}
		}
		cur_dtmf_tone = DTMF_ACK;
	} else if (cmd1 == 'D' && cmd2 == 'A') {
		// Main data reception
		cur_dtmf_tone = -1;
		uint8_t parity = 0;
		uint16_t dataLen = ((sysex_data[6] & 0x7F) << 7) | (sysex_data[7] & 0x7F);
		parsedDataLength = dataLen;

		uint16_t parsedDataPos = 0;

		for (uint16_t i = 8; i < sysex_length-1; i+=8) {
			parity ^= sysex_data[i+0];
			parity ^= sysex_data[i+1];
			parity ^= sysex_data[i+2];
			parity ^= sysex_data[i+3];
			parity ^= sysex_data[i+4];
			parity ^= sysex_data[i+5];
			parity ^= sysex_data[i+6];
			parity ^= sysex_data[i+7];

			parsedData[parsedDataPos++] = sysex_data[i+0] | ((sysex_data[i+7] & 0b1) << 7);
			parsedData[parsedDataPos++] = sysex_data[i+1] | ((sysex_data[i+7] & 0b10) << 6);
			parsedData[parsedDataPos++] = sysex_data[i+2] | ((sysex_data[i+7] & 0b100) << 5);
			parsedData[parsedDataPos++] = sysex_data[i+3] | ((sysex_data[i+7] & 0b1000) << 4);
			parsedData[parsedDataPos++] = sysex_data[i+4] | ((sysex_data[i+7] & 0b10000) << 3);
			parsedData[parsedDataPos++] = sysex_data[i+5] | ((sysex_data[i+7] & 0b100000) << 2);
			parsedData[parsedDataPos++] = sysex_data[i+6] | ((sysex_data[i+7] & 0b1000000) << 1);
		}

		last_checksum = parity;

		if (parsedDataLength != parsedDataPos) {cur_dtmf_tone = DTMF_NAK; return;}

		uint8_t parsed_parity = 0;
		for(uint16_t i = 0; i < parsedDataLength - 1; i++) {
			parsed_parity ^= parsedData[i];
		}
		if(parsed_parity != parsedData[parsedDataLength-1]) {cur_dtmf_tone = DTMF_NAK; return;}

		if (sysex_data[sysex_length - 1] != parity) {
			cur_dtmf_tone = DTMF_NAK;
		} else {
			cur_dtmf_tone = DTMF_ACK;
		}
	}
}

void USART1_IRQHandler() {
	static volatile uint8_t receiving_sysex = 0;

	uint8_t rb = USART1->RDR;

	if(USART1->ISR & USART_ISR_ORE) {
		receiving_sysex = 0;
	}
	USART1->ICR |= USART_ICR_ORECF;

	if(((USART1->ISR) & USART_ISR_ORE) != 0) {
		receiving_sysex = 0;
		return;
	}

	if (rb == 0xF0) {receiving_sysex = 1; sysex_data_pos = 0;}
	else if (rb == 0xF7) {
			receiving_sysex = 0;

			if (sysex_data_pos > 6) {
				if (sysex_data[0] == 0x00 && sysex_data[1] == 0x23 && sysex_data[2] == 0x07 && sysex_data[3] == 0x01) {
					parse_sysex();
				}
			}

			parse_sysex();
	} else if ((rb & 0x80) != 0) {
		// Something bad happened.
		receiving_sysex = 0;
		sysex_data_pos = 0;
	}  else if (receiving_sysex) {
		sysex_data[sysex_data_pos++] = rb;
		if (sysex_data_pos > 1200) {sysex_data_pos = 1200;}
	}
}
