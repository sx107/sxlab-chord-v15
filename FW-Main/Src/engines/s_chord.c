/*
 * s_chord.c
 *
 *  Created on: 10 дек. 2024 г.
 *      Author: Sx107
 */

#include "synth.h"
#include "synth_defines.h"
#include "synth_lut.h"
#include "engines/s_chord.h"

#include <pwm.h>
#include <adc.h>
#include <lfo.h>
#include <midi.h>


static volatile uint16_t _synth_spread_raw;
static volatile uint16_t _synth_scale_raw;
static volatile uint8_t _synth_spread_prev = 0xFF;
static volatile uint8_t _synth_scale_prev = 0xFF;

void synth_chord_update(uint8_t reload) {
	uint16_t spread_r = _synth_knob[ADC_SPREAD];
	uint16_t spread_v = spread_r / OSC_SPREAD_DIV;
	uint16_t spread_h = spread_r % OSC_SPREAD_DIV;

	if(((spread_h > OSC_SPREAD_HYST) && (spread_h < OSC_SPREAD_DIV - OSC_SPREAD_HYST) && _synth_spread_prev != spread_v ) || (reload == 1)) {
		_synth_spread_raw = spread_v * OSC_SPREAD_DIV + OSC_SPREAD_HDIV;
		_synth_spread_prev = spread_v;
	}

	uint16_t scale_r = _synth_knob[ADC_SCALE];
	uint16_t scale_v = scale_r / OSC_SCALE_DIV;
	uint16_t scale_h = scale_r % OSC_SCALE_DIV;

	if(((scale_h > OSC_SCALE_HYST) && (scale_h < OSC_SCALE_DIV - OSC_SCALE_HYST) && _synth_scale_prev != scale_v) || (reload == 1)) {
		_synth_scale_raw = scale_v * OSC_SCALE_DIV + OSC_SCALE_HDIV;
		_synth_scale_prev = scale_v;
	}
}

void synth_chord_process() {
	static volatile uint32_t _synth_phase[NOSC];

	uint32_t so = 0;

	//Waveform
	int16_t raw_wf = _synth_knob[ADC_WF] + lfoValue[LFO_TGT_WF] + (_synth_mod << 5);
	if (raw_wf < 0) {raw_wf = 0;}
	else if (raw_wf > 4095) {raw_wf = 4095;}

	uint8_t is_square = 0;
	uint16_t pwm = 0;
	uint32_t wf = 1023;

	if(_synth_easter_square) {
		if(raw_wf < 2048) {
			raw_wf *= 2;
			wf = 1023-(raw_wf >> 2);
			wf = wf_lut[wf];
		} else {
			is_square = 1;
			pwm = raw_wf - 2048;
			wf = 0;
		}
	} else {
		wf = 1023-(raw_wf >> 2);
		wf = wf_lut[wf];
		// wf = (wf * 3500) >> 12;
	}

	//Coarse pitch
	/*
	int16_t coarsePitch = _synth_knob[ADC_COAR_PITCH] + lfoValue[LFO_TGT_COARSE];
	if(coarsePitch < 0) {coarsePitch = 0;}
	if(coarsePitch > 4095) {coarsePitch = 4095;}
	uint8_t coarseIndex = coarsePitch >> 7;
	*/
	if(midi_get_monophonic() != 0xFF) {_synth_last_midi = midi_get_monophonic();}
	uint32_t basePitch = midi_lut[57 - SYNTH_MIDI_BASE];
	if(_synth_last_midi != 0xFF) {basePitch = midi_lut[_synth_last_midi-SYNTH_MIDI_BASE];}

	//uint32_t basePitch = coarse_lut[coarseIndex];

	//Fine pitch
	int32_t finePitch = _synth_knob[ADC_FINE_PITCH] + lfoValue[LFO_TGT_FINE] + (_synth_cc[SYNTH_CC_FINE] << 5);
	if(finePitch < 0) {finePitch = 0;}
	else if(finePitch > 4095) {finePitch = 4095;}
	uint16_t lut_fine_low = fine_lut[finePitch >> 4];
	uint16_t lut_fine_high = fine_lut[(finePitch >> 4) + 1];
	uint16_t fp_ready = lut_fine_low + ((lut_fine_low - lut_fine_high) * (finePitch & 0b1111) >> 4);
	basePitch = basePitch * fp_ready >> 14;

	//Coarse
	int32_t coarsePitch = (_synth_knob[ADC_COAR_PITCH]) + lfoValue[LFO_TGT_COARSE] + (_synth_cc[SYNTH_CC_COARSE] << 5);
	if(coarsePitch < 0) {coarsePitch = 0;}
	else if(coarsePitch > 4095) {coarsePitch = 4095;}
	uint8_t cp_ready = coarsePitch / 164;
	if(cp_ready > 24) {cp_ready = 24;}
	basePitch = basePitch * coarse_lut[cp_ready] >> 14;


	//Pitchbend
	finePitch = _synth_pb >> 2;
	if(finePitch < 0) {finePitch = 0;}
	else if(finePitch > 4095) {finePitch = 4095;}
	lut_fine_low = pitchbend_lut[finePitch >> 4];
	lut_fine_high = pitchbend_lut[(finePitch >> 4) + 1];
	fp_ready = lut_fine_low + ((lut_fine_low - lut_fine_high) * (finePitch & 0b1111) >> 4);
	basePitch = basePitch * fp_ready >> 14;

	//Unison
	int16_t unison = _synth_knob[ADC_UNI] + lfoValue[LFO_TGT_UNI] + (_synth_cc[SYNTH_CC_UNISON] << 5);
	if(unison < 0) {unison = 0;}
	else if (unison > 4095) {unison = 4095;}
	uint16_t uni_spread = 0;
	if(unison > (2048+UNISON_NULL)) {
		uni_spread = (unison - (2048+UNISON_NULL)) >> 2;
		uni_spread = uni_lut[uni_spread];
	}

	//Spread
	int16_t spread_v = _synth_spread_raw + lfoValue[LFO_TGT_SPREAD] + (_synth_cc[SYNTH_CC_SPREAD] << 5);
	if(spread_v < 0) {spread_v = 0;}
	else if (spread_v > 4095) {spread_v = 4095;}
	spread_v /= OSC_SPREAD_DIV;

	//Scale
	int16_t scale_v = _synth_scale_raw + lfoValue[LFO_TGT_SCALE] + (_synth_cc[SYNTH_CC_SCALE] << 5);
	if(scale_v < 0) {scale_v = 0;}
	else if (scale_v > 4095) {scale_v = 4095;}
	scale_v /= OSC_SCALE_DIV;


	for(uint8_t i = 0; i < NOSC; i++) {
		int16_t uni_index = i - 2;
		uint32_t cur_phinc = basePitch;
		//cur_phinc = cur_phinc * (1 + spread_lut[spread_v][i]);

		if(/*i != 0 && unison < 2*/ 0) {
			_synth_phase[i] = _synth_phase[0];
		} else {
			cur_phinc = cur_phinc * scales_lut[scale_v][spread_lut[spread_v][i]] >> 13;
			if(unison >= 2048) {
				cur_phinc = cur_phinc * (1024 + uni_spread * uni_index) >> 10;
			} else {
				uint32_t bp = basePitch * scales_lut[scale_v][0] >> 13;
				//bp += basePitch * scales_lut[scale_v][spread_lut[spread_v][5]] >> 13;
				//bp/=2;
				cur_phinc = bp + ((cur_phinc - bp)*(unison) >> 11);
			}
			_synth_phase[i] += cur_phinc;
		}


		uint32_t v = (_synth_phase[i] >> 6) & 2047;

		if(!is_square) {
			if(v < (wf)) {v = v * 1023 / wf;}
			else {v = 1023 - (v-wf)*1023/(2048-wf);}
		} else {
			if (pwm < 512) {
				uint32_t v_saw = 1023 - (v >> 1);
				uint32_t v_square = v < 1024 ? 1023 : 0;
				v = (v_saw * (511 - pwm) + v_square * pwm) >> 9;
			} else {
				v = v < 1024+((pwm - 512) >> 1) ? 1023 : 0;
			}
		}

		/*
		#ifdef SAWMODE
			if(v < (wf)) {v = v * 1023 / wf;}
			else {v = 1023 - (v-wf)*1023/(2047-wf);}
		#else
			if(v < 1023) {v = (v * wf >> 10);}
			else {v = 1023 - ((v -1024) * wf >> 10);}
		#endif
		*/

		//if(i == 0) {so = v;}


		if (unison > UNI_PHASE_TRANSITION) {
			so += v;
		} else {
			if(i == 0) {
				so += (v * (UNI_PHASE_TRANSITION + (UNI_PHASE_TRANSITION - unison))) >> 8;
			} else {
				so += (v * unison) >> 8;
			}
		}
	}

	if(_synth_drone || midi_get_monophonic() != 0xFF) {
		so /= 7;
		if(so < 0) {so = 0;}
		if(so > 1023) {so = 1023;}
		PWM_AUDIO_OUT = so;
	} else {
		//PWM_AUDIO_OUT = 0;
	}
}
