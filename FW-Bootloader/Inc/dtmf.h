/*
 * dtmf.h
 *
 *  Created on: Sep 29, 2025
 *      Author: Sx107
 */

#ifndef DTMF_H_
#define DTMF_H_

#include <main.h>

void dtmf_configure();
extern volatile int8_t cur_dtmf_tone;

#define PWM_AUDIO_OUT (TIM1->CCR1)
#define PWM_LED1 (TIM3->CCR4) // AKA LED_WF3
#define PWM_LED2 (TIM1->CCR2) // AKA LED_WF2
#define PWM_LED3 (TIM1->CCR3) // AKA LED_WF1
#define PWM_LED4 (TIM1->CCR4) // AKA LED_WF6
#define PWM_LED5 (TIM3->CCR1) // AKA LED_WF5
#define PWM_LED6 (TIM3->CCR2) // AKA LED_WF4

#define DTMF_SILENCE -1
#define DTMF_DIALTONE 0
#define DTMF_ACK 1
#define DTMF_WTF 2
#define DTMF_RESEND 3
#define DTMF_FLASH_ERROR 4
#define DTMF_EOF 5
#define DTMF_WAIT 6
#define DTMF_NAK 7



#endif /* DTMF_H_ */
