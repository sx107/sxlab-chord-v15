/*
 * sysex.h
 *
 *  Created on: Sep 29, 2025
 *      Author: Sx107
 */

#ifndef SYSEX_H_
#define SYSEX_H_

void sysex_configure();

typedef enum {
	SYSEX_MODE_DEFAULT, // Not sending any data, reset state machine.
	SYSEX_WAIT_FIRST, // Waiting for the first ACK
	SYSEX_WAIT_ACK, // Waiting for ACK, then send next tone
	SYSEX_WAIT_EOF, // Waiting of EOF confirmed message
	SYSEX_WAIT_ACK_RESEND
} sysex_mode_t;

#endif /* SYSEX_H_ */
