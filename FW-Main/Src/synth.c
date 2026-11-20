/*
 * synth.c
 *
 *  Created on: Jan 12, 2022
 *      Author: Sx107
 */

#include <main.h>
#include <clock.h>
#include <pwm.h>
#include <synth.h>
#include <adc.h>
#include <lfo.h>
#include <synth_lut.h>
#include <stm32f0xx.h>
#include <eeprom.h>
#include <midi.h>

#include "synth_defines.h"
#include "engines/s_chord.h"
#include "engines/s_arp_chord.h"
#include "engines/s_arp.h"

volatile uint8_t synth_engine = 0;

volatile uint16_t _synth_knob_hlast[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t _synth_knob_active[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint16_t _synth_knob[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};


volatile uint8_t _synth_drone = 1;
volatile uint8_t _synth_midisync = 0;

volatile uint16_t _synth_pb = 8192;
volatile uint8_t _synth_mod = 0;
volatile uint8_t _synth_last_midi = 0xFF;
volatile uint8_t _synth_cc[5] = {0, 0, 0, 0, 0};

volatile uint8_t _synth_easter_square = 0;
volatile uint8_t _synth_easter_tuning = 0;
volatile uint8_t _synth_easter_lfohp = 0;


void synth_load_preset(uint8_t preset) {
	for(uint8_t i = 0; i < 9; i++) {
		_synth_knob_active[i] = 0;
		_synth_knob_hlast[i] = adcData[i];
	}
	uint8_t data[48];
	eeprom_read_buffer(preset * 48, data, 16);
	eeprom_read_buffer(preset * 48+16, data+16, 16);
	eeprom_read_buffer(preset * 48+32, data+32, 16);

	_synth_knob[ADC_FINE_PITCH] = 	*(uint16_t*)(&data[0]);
	_synth_knob[ADC_COAR_PITCH] = 	*(uint16_t*)(&data[2]);
	_synth_knob[ADC_UNI] 		= 	*(uint16_t*)(&data[4]);
	_synth_knob[ADC_WF] 		= 	*(uint16_t*)(&data[6]);
	_synth_knob[ADC_SCALE] 		= 	*(uint16_t*)(&data[8]);
	_synth_knob[ADC_SPREAD] 	= 	*(uint16_t*)(&data[10]);

	lfo_panic();
	for(uint8_t i = 0; i < 6; i++) {
		_lfo_wf[i] = *(uint16_t*)(&data[12+i*4]);
		_lfo_rate[i] = *(uint16_t*)(&data[12+i*4+2]);
	}

	// Other data starts at 6*2 + 6*4 = 36
	_synth_drone = data[36];
	_synth_midisync = data[37];

	switch(synth_engine) {
		case 0: synth_chord_update(1); break;
		case 1: synth_arp_chord_update(1); break;
		case 2: synth_arp_update(1); break;
	}
}
void synth_save_preset(uint8_t preset) {
	//eeprom_wait_available(100);

	uint8_t data[48];

	*(uint16_t*)(&data[0]) = _synth_knob[ADC_FINE_PITCH];
	*(uint16_t*)(&data[2]) = _synth_knob[ADC_COAR_PITCH];
	*(uint16_t*)(&data[4]) = _synth_knob[ADC_UNI];
	*(uint16_t*)(&data[6]) = _synth_knob[ADC_WF];
	*(uint16_t*)(&data[8]) = _synth_knob[ADC_SCALE];
	*(uint16_t*)(&data[10]) = _synth_knob[ADC_SPREAD];

	for(uint8_t i = 0; i < 6; i++) {
		*(uint16_t*)(&data[12+i*4]) = _lfo_wf[i];
		*(uint16_t*)(&data[12+i*4+2]) = _lfo_rate[i];
	}

	data[36] = _synth_drone;
	data[37] = _synth_midisync;

	eeprom_write_buffer(preset * 48, data, 16);
	eeprom_wait_available(100);
	eeprom_write_buffer(preset * 48+16, data + 16, 16);
	eeprom_wait_available(100);
	eeprom_write_buffer(preset * 48+32, data + 32, 16);
	eeprom_wait_available(100);
	_delayMs(10);
}
void synth_set_engine(uint8_t engine) {
	uint8_t new_engine = synth_engine;
	if (engine <= 2) {
		new_engine = engine;
	}

	uint8_t prev_engine = synth_engine;
	synth_engine = new_engine;

	if(prev_engine != engine) {
		switch(synth_engine) {
			case 0: synth_chord_update(1); break;
			case 1: synth_arp_chord_update(1); break;
			case 2: synth_arp_update(1); synth_arp_update_midi_stack(0xFF); break;
		}
	}
}

void synth_set_pb(uint16_t pb) {
	_synth_pb = pb;
}
void synth_set_mod(uint8_t mod) {
	_synth_mod = mod;
}
void synth_set_cc(uint8_t id, uint8_t cc) {
	_synth_cc[id] = cc;
}
void synth_reset_midi() {
	_synth_pb = 8192;
	_synth_mod = 0;
	_synth_last_midi = 0xFF;
	for(uint8_t i = 0; i < 5; i++) {
		_synth_cc[i] = 0;
	}
	synth_arp_reset_midi();
	synth_arp_update_midi_stack(0xFF);
}

void synth_update() {
	static uint8_t first_time = 1;

	for(uint8_t i = 0; i < 9; i++) {
		int16_t h = adcData[i] - _synth_knob_hlast[i];
		if (h > SYNTH_POT_HYST || h < -SYNTH_POT_HYST) {_synth_knob_active[i] = 1;}
		if(_synth_knob_active[i]) {_synth_knob[i] = adcData[i];}
	}

	switch(synth_engine) {
		case 0: synth_chord_update(first_time); break;
		case 1: synth_arp_chord_update(first_time); break;
		case 2: synth_arp_update(first_time); break;
	}

	first_time = 0;
}

void synth_process() {
	switch(synth_engine) {
		case 0: synth_chord_process(); break;
		case 1: synth_arp_chord_process(); break;
		case 2: synth_arp_process(); break;
	}
}


