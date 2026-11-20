/*
 * s_arp.h
 *
 *  Created on: 10 дек. 2024 г.
 *      Author: Sx107
 */

#ifndef ENGINES_S_ARP_H_
#define ENGINES_S_ARP_H_

#include <main.h>

void synth_arp_update_midi_stack(uint8_t add_note);
void synth_arp_reset_midi();
void synth_arp_update(uint8_t reload);
void synth_arp_process();

#endif /* ENGINES_S_ARP_H_ */
