/*
 * s_arp_chord.c
 *
 *  Created on: 10 дек. 2024 г.
 *      Author: Sx107
 */


#include "synth.h"
#include "synth_defines.h"
#include "synth_lut.h"
#include "engines/s_arp_chord.h"

#include <pwm.h>
#include <adc.h>
#include <lfo.h>
#include <midi.h>

static volatile uint16_t _synth_scale_raw;
static volatile uint8_t _synth_scale_prev = 0xFF;

static volatile uint16_t _synth_fpsel_raw;
static volatile uint8_t _synth_fpsel_prev = 0xFF;
static volatile uint8_t _synth_fpsel_prev_proc = 0xFF;

void synth_arp_chord_update(uint8_t reload) {
	uint16_t scale_r = _synth_knob[ADC_SCALE];
	uint16_t scale_v = scale_r / OSC_SCALE_DIV;
	uint16_t scale_h = scale_r % OSC_SCALE_DIV;

	if(((scale_h > OSC_SCALE_HYST) && (scale_h < OSC_SCALE_DIV - OSC_SCALE_HYST) && _synth_scale_prev != scale_v) || (reload == 1)) {
		_synth_scale_raw = scale_v * OSC_SCALE_DIV + OSC_SCALE_HDIV;
		_synth_scale_prev = scale_v;
	}

	uint16_t fp_r = _synth_knob[ADC_SPREAD];
	uint16_t fp_v = fp_r / OSC_FPSEL_DIV;
	uint16_t fp_h = fp_r % OSC_FPSEL_DIV;

	if(((fp_h > OSC_FPSEL_HYST) && (fp_h < OSC_FPSEL_DIV - OSC_FPSEL_HYST) && _synth_fpsel_prev != fp_v) || (reload == 1)) {
		_synth_fpsel_raw = fp_v * OSC_FPSEL_DIV + OSC_FPSEL_HDIV;
		_synth_fpsel_prev = fp_v;
	}
	if(reload == 1) {
		_synth_fpsel_prev_proc = fp_v;
	}
}

void synth_arp_chord_process() {
	static volatile uint32_t _synth_phase[NOSC];
	static volatile int32_t _synth_vol[NOSC] = {0, 0, 0, 0, 0, 0};



	//Waveform
	int16_t raw_wf = _synth_knob[ADC_WF] + lfoValue[LFO_TGT_WF] + (_synth_mod << 5);
	if (raw_wf < 0) {raw_wf = 0;}
	else if (raw_wf > 4095) {raw_wf = 4095;}

	uint8_t is_square = 0;
	uint16_t pwm = 0;
	uint32_t wf = 1023;

	#if ENABLE_SQUARE
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
			wf = (wf * 3500) >> 12;
		}
	#else
		wf = 1023-(raw_wf >> 2);
		wf = wf_lut[wf];
		wf = (wf * 3500) >> 12;
	#endif



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

	//Spread
	int16_t spread_v = _synth_fpsel_raw + lfoValue[LFO_TGT_SPREAD] + (_synth_cc[SYNTH_CC_SPREAD] << 5);
	if(spread_v < 0) {spread_v = 0;}
	else if (spread_v > 4095) {spread_v = 4095;}

	uint8_t fpsel = spread_v / OSC_FPSEL_DIV;
	if((fpsel != _synth_fpsel_prev_proc) || (midi_get_monophonic() != 0xFF)) {
		_synth_vol[fpsel] = VOLUME_MAX;
	}
	_synth_fpsel_prev_proc = fpsel;


	//uint16_t lut_fine_low = fine_lut[finePitch >> 4];
	//uint16_t lut_fine_high = fine_lut[(finePitch >> 4) + 1];
	//uint16_t fp_ready = lut_fine_low + ((lut_fine_low - lut_fine_high) * (finePitch & 0b1111) >> 4);
	//basePitch = basePitch * fp_ready >> 14;

	//Coarse
	int32_t coarsePitch = (_synth_knob[ADC_COAR_PITCH]) + lfoValue[LFO_TGT_COARSE] + (_synth_cc[SYNTH_CC_COARSE] << 5);
	if(coarsePitch < 0) {coarsePitch = 0;}
	else if(coarsePitch > 4095) {coarsePitch = 4095;}
	uint8_t cp_ready = coarsePitch / 164;
	if(cp_ready > 24) {cp_ready = 24;}
	basePitch = basePitch * coarse_lut[cp_ready] >> 14;

	//Fine pitch
	int32_t finePitch = _synth_knob[ADC_FINE_PITCH] + lfoValue[LFO_TGT_FINE] + (_synth_cc[SYNTH_CC_FINE] << 5);
	if(finePitch < 0) {finePitch = 0;}
	else if(finePitch > 4095) {finePitch = 4095;}
	uint16_t lut_fine_low = fine_lut[finePitch >> 4];
	uint16_t lut_fine_high = fine_lut[(finePitch >> 4) + 1];
	uint16_t fp_ready = lut_fine_low + ((lut_fine_low - lut_fine_high) * (finePitch & 0b1111) >> 4);
	basePitch = basePitch * fp_ready >> 14;


	//Pitchbend
	int32_t finePB = _synth_pb >> 2;
	if(finePB < 0) {finePB = 0;}
	else if(finePB > 4095) {finePB = 4095;}
	uint16_t lut_pb_low = pitchbend_lut[finePB >> 4];
	uint16_t lut_pb_high = pitchbend_lut[(finePB >> 4) + 1];
	uint16_t pb_ready = lut_pb_low + ((lut_pb_low - lut_pb_high) * (finePB & 0b1111) >> 4);
	basePitch = basePitch * pb_ready >> 14;

	//Unison
	int16_t unison = _synth_knob[ADC_UNI] + lfoValue[LFO_TGT_UNI] + (_synth_cc[SYNTH_CC_UNISON] << 5);
	if(unison < 0) {unison = 0;}
	else if (unison > 4095) {unison = 4095;}
	uint16_t decay = 1023 - (unison >> 2);
	decay = wf_lut[decay] >> 1;



	//Scale
	int16_t scale_v = _synth_scale_raw + lfoValue[LFO_TGT_SCALE] + (_synth_cc[SYNTH_CC_SCALE] << 5);
	if(scale_v < 0) {scale_v = 0;}
	else if (scale_v > 4095) {scale_v = 4095;}
	scale_v /= OSC_SCALE_DIV;


	// Sound generation
	int32_t so = 0;

	for(uint8_t i = 0; i < NOSC; i++) {
		uint32_t cur_phinc = basePitch;
		//cur_phinc = cur_phinc * (1 + spread_lut[spread_v][i]);

		if(/*i != 0 && unison < 2*/ 0) {
			_synth_phase[i] = _synth_phase[0];
		} else {
			cur_phinc = cur_phinc * scales_lut[scale_v][i] >> 13;
			cur_phinc = (cur_phinc * 1025) >> 10;
			_synth_phase[i] += cur_phinc;
		}


		int32_t v = (_synth_phase[i] >> 6) & 2047;


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

		v -= 511;

		if (_synth_drone) {
			_synth_vol[i] = VOLUME_MAX;
		} else {
			v = (v * _synth_vol[i]) >> VOLUME_PRECISION;
			_synth_vol[i] = (_synth_vol[i] * (32767 - decay)) >> 15;
		}


		so += v;
	}

	so /= 6;
	so += 512;
	if(so < 0) {so = 0;}
	if(so > 1023) {so = 1023;}
	PWM_AUDIO_OUT = so;
}
