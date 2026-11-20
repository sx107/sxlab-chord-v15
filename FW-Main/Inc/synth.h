/*
 * synth.h
 *
 *  Created on: Jan 12, 2022
 *      Author: Sx107
 */

#ifndef SYNTH_H_
#define SYNTH_H_

#include <main.h>

extern volatile uint8_t synth_engine;

extern volatile uint8_t _synth_drone;
extern volatile uint8_t _synth_midisync;
extern volatile uint8_t _synth_knob_active[9];
extern volatile uint16_t _synth_knob[9];

extern volatile uint16_t _synth_pb;
extern volatile uint8_t _synth_mod;
extern volatile uint8_t _synth_last_midi;
extern volatile uint8_t _synth_cc[5];

extern volatile uint8_t _synth_easter_square;
extern volatile uint8_t _synth_easter_tuning;
extern volatile uint8_t _synth_easter_lfohp;

void synth_process();
void synth_update();

void synth_set_engine(uint8_t engine);

void synth_load_preset(uint8_t preset);
void synth_save_preset(uint8_t preset);

void synth_set_pb(uint16_t pb);
void synth_set_mod(uint8_t mod);
void synth_set_cc(uint8_t id, uint8_t cc);
void synth_reset_midi();

#define SYNTH_CC_COARSE 0
#define SYNTH_CC_FINE 1
#define SYNTH_CC_UNISON 2
#define SYNTH_CC_SCALE 3
#define SYNTH_CC_SPREAD 4


#endif /* SYNTH_H_ */
