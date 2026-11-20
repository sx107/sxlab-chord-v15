/*
 * synth_defines.h
 *
 *  Created on: 10 дек. 2024 г.
 *      Author: Sx107
 */

#ifndef SYNTH_DEFINES_H_
#define SYNTH_DEFINES_H_

#define NOSC 6

#define SAWMODE 1
#define ENABLE_SQUARE 1

#define OSC_SPREAD_DIV 683
#define OSC_SPREAD_HDIV 341
#define OSC_SPREAD_HYST 200

#define OSC_SCALE_DIV 512
#define OSC_SCALE_HDIV 256
#define OSC_SCALE_HYST 100

#define OSC_SCALE_ARP_DIV 456
#define OSC_SCALE_ARP_HDIV 228
#define OSC_SCALE_ARP_HYST 100

#define OSC_FPSEL_DIV 683
#define OSC_FPSEL_HDIV 0
#define OSC_FPSEL_HYST 20

#define VOLUME_PRECISION 16
#define VOLUME_MAX ((1 << VOLUME_PRECISION) - 1)

#define SYNTH_POT_HYST 150
#define UNISON_NULL 100
#define UNI_PHASE_TRANSITION 256

#define SYNTH_MIDI_BASE 21
#define SYNTH_MIDI_MAX 81

#endif /* SYNTH_DEFINES_H_ */
