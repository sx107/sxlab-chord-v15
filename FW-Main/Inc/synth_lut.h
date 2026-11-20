/*
 * synth_lut.h
 *
 *  Created on: Jan 14, 2022
 *      Author: Sx107
 */

#ifndef SYNTH_LUT_H_
#define SYNTH_LUT_H_

#include <main.h>

extern const uint16_t wf_lut[];
extern const uint16_t uni_lut[];

extern const uint16_t midi_lut[];
extern const uint16_t coarse_lut[];
extern const uint16_t fine_lut[];
extern const uint16_t pitchbend_lut[];
extern const uint8_t spread_lut[6][6];
extern const uint8_t fm_index_lut[8][6];
extern const uint32_t scales_lut[8][6];

// const uint8_t* arp_scales_quantize_lut[8];
const uint8_t arp_scales_notes_length[9];
const uint8_t arp_scales_notes_lut[9][25];
const uint16_t arp_scales_notes_lengthdiv[9];

#endif /* SYNTH_LUT_H_ */
