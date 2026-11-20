/*
 * s_arp.c
 *
 *  Created on: 10 дек. 2024 г.
 *      Author: Sx107
 */


#include "synth.h"
#include "synth_defines.h"
#include "synth_lut.h"
#include "engines/s_arp.h"

#include <pwm.h>
#include <adc.h>
#include <lfo.h>
#include <midi.h>

static volatile uint16_t _synth_spread_raw;
static volatile uint16_t _synth_scale_raw;
static volatile uint8_t _synth_spread_prev = 0xFF;
static volatile uint8_t _synth_scale_prev = 0xFF;

static volatile uint16_t _synth_fpsel_raw;
static volatile uint8_t _synth_fpsel_prev = 0xFF;
static volatile uint8_t _synth_fpsel_prev_proc = 0xFF;

static volatile uint32_t _synth_wf_prev = 0;
static volatile uint32_t _synth_wf_raw = 0;

static volatile uint8_t _synth_note[NOSC] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static volatile int32_t _synth_vol[NOSC] = {0, 0, 0, 0, 0, 0};
static volatile uint8_t _synth_is_osc_pressed[NOSC] = {0, 0, 0, 0, 0, 0};
static volatile uint8_t _synth_note_forward_memory[128] = {0xFF};
static volatile uint8_t _synth_notes_rotate = 0;

void synth_arp_reset_midi() {
	for(uint8_t i = 0; i < 128; i++) {_synth_note_forward_memory[i] = 0xFF;}
}

void synth_arp_update_midi_stack(uint8_t add_note) {
	uint8_t _synth_midi_tmp_notes[NOSC];
	uint8_t is_playing[NOSC] = {0, 0, 0, 0, 0, 0};
	uint8_t notes_not_found[NOSC];
	uint8_t nnf_index = 0;
	uint8_t n_not_found = 0;
	uint8_t n_free_osc = NOSC;

	if (add_note != 0xFF) {
		_synth_midi_tmp_notes[0] = add_note;
		midi_get_polyphonic(_synth_midi_tmp_notes + 1, NOSC - 1);
	} else {
		midi_get_polyphonic(_synth_midi_tmp_notes, NOSC);
	}


	// Process already playing notes, push every not found note into notes_not_found
	for (uint8_t i = 0; i < NOSC; i++) {
		uint8_t n = _synth_midi_tmp_notes[i];
		if (n == 0xFF) {break;}

		if (_synth_note_forward_memory[n] != 0xFF && _synth_note[_synth_note_forward_memory[n]] == n) {
			uint8_t osc = _synth_note_forward_memory[n];

			if (n == add_note && add_note != 0xFF) {_synth_vol[osc] = VOLUME_MAX >> 1;}
			else {
				_synth_vol[osc] = VOLUME_MAX - (((int32_t)(127 - midi_get_velocity(n))) << (VOLUME_PRECISION - 7));
				is_playing[osc] = 1;
			}
			_synth_note[osc] = n;
			n_free_osc--;
		} else {
			notes_not_found[nnf_index++] = n;
		}
	}

	n_not_found = nnf_index;

	// Find free oscillators, fill them with notes!
	if (n_not_found != 0 && n_free_osc != 0) {
		nnf_index = 0; // New index to iterate through notes_not_found
		volatile uint8_t new_rotate = 0xFF;

		for(uint8_t i = 0; i < NOSC; i++) {
			uint8_t osc_index = (i + _synth_notes_rotate) % NOSC;
			if (!is_playing[osc_index]) {
				new_rotate = osc_index;

				if (nnf_index >= n_not_found) {break;}
				uint8_t n = notes_not_found[nnf_index++];
				if (n == 0xFF) {break;}

				if (n == add_note && add_note != 0xFF) {_synth_vol[osc_index] = VOLUME_MAX >> 1;}
				else {
					_synth_vol[osc_index] =  VOLUME_MAX - (((int32_t)(127 - midi_get_velocity(n))) << (VOLUME_PRECISION - 7));
					is_playing[osc_index] = 1;
				}
				_synth_note[osc_index] = n;
				_synth_note_forward_memory[n] = osc_index;
			}
		}

		if(new_rotate != 0xFF) {_synth_notes_rotate = new_rotate;}
	}

	for (uint8_t i = 0; i < NOSC; i++) {
		_synth_is_osc_pressed[i] = is_playing[i];
	}
}

void synth_arp_update(uint8_t reload) {
	uint16_t scale_r = _synth_knob[ADC_SCALE];
	uint16_t scale_v = scale_r / OSC_SCALE_ARP_DIV;
	uint16_t scale_h = scale_r % OSC_SCALE_ARP_DIV;

	if(((scale_h > OSC_SCALE_ARP_HYST) && (scale_h < OSC_SCALE_ARP_DIV - OSC_SCALE_ARP_HYST) && _synth_scale_prev != scale_v) || (reload == 1)) {
		_synth_scale_raw = scale_v * OSC_SCALE_ARP_DIV + OSC_SCALE_ARP_HDIV;
		_synth_scale_prev = scale_v;
	}

	uint16_t fpsel_div = arp_scales_notes_lengthdiv[_synth_scale_prev];
	uint16_t fpsel_hdiv = fpsel_div >> 1;

	uint16_t fp_r = _synth_knob[ADC_SPREAD];
	uint16_t fp_v = fp_r / fpsel_div;
	uint16_t fp_h = fp_r % fpsel_div;

	if(((fp_h > OSC_FPSEL_HYST) && (fp_h < fpsel_div - OSC_FPSEL_HYST) && _synth_fpsel_prev != fp_v) || (reload == 1)) {
		_synth_fpsel_raw = fp_v * fpsel_div + fpsel_hdiv;
		_synth_fpsel_prev = fp_v;
	}
	if(reload == 1) {
		_synth_fpsel_prev_proc = fp_v;
	}
}

void synth_arp_process() {
	static volatile uint32_t _synth_phase[NOSC];

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

	//Scale
	int16_t scale_v = _synth_scale_raw + lfoValue[LFO_TGT_SCALE] + (_synth_cc[SYNTH_CC_SCALE] << 5);
	if(scale_v < 0) {scale_v = 0;}
	else if (scale_v > 4095) {scale_v = 4095;}
	scale_v /= OSC_SCALE_ARP_DIV;

	//Coarse
	int32_t coarsePitch = (_synth_knob[ADC_COAR_PITCH]) + lfoValue[LFO_TGT_COARSE] + (_synth_cc[SYNTH_CC_COARSE] << 5);
	if(coarsePitch < 0) {coarsePitch = 0;}
	else if(coarsePitch > 4095) {coarsePitch = 4095;}
	uint8_t cp_ready = coarsePitch / 164;
	if(cp_ready > 24) {cp_ready = 24;}

	// FPSEL
	int16_t spread_v = _synth_fpsel_raw + lfoValue[LFO_TGT_SPREAD] + (_synth_cc[SYNTH_CC_SPREAD] << 5);
	if(spread_v < 0) {spread_v = 0;}
	else if (spread_v > 4095) {spread_v = 4095;}

	uint8_t fpsel = spread_v / arp_scales_notes_lengthdiv[scale_v];
	if(fpsel >= arp_scales_notes_length[scale_v]) {fpsel = arp_scales_notes_length[scale_v] - 1;}
	if(fpsel != _synth_fpsel_prev_proc) {
		synth_arp_update_midi_stack(57 + cp_ready - 24 + arp_scales_notes_lut[scale_v][fpsel]);
	}
	_synth_fpsel_prev_proc = fpsel;

	//Unison
	int16_t unison = _synth_knob[ADC_UNI] + lfoValue[LFO_TGT_UNI] + (_synth_cc[SYNTH_CC_UNISON] << 5);
	if(unison < 0) {unison = 0;}
	else if (unison > 4095) {unison = 4095;}
	uint16_t decay = 1023 - (unison >> 2);
	decay = wf_lut[decay] >> 1;

	//Fine pitch
	int32_t finePitch = _synth_knob[ADC_FINE_PITCH] + lfoValue[LFO_TGT_FINE] + (_synth_cc[SYNTH_CC_FINE] << 5);
	if(finePitch < 0) {finePitch = 0;}
	else if(finePitch > 4095) {finePitch = 4095;}
	uint16_t lut_fine_low = fine_lut[finePitch >> 4];
	uint16_t lut_fine_high = fine_lut[(finePitch >> 4) + 1];
	uint16_t fp_ready = lut_fine_low + ((lut_fine_low - lut_fine_high) * (finePitch & 0b1111) >> 4);
	//basePitch = basePitch * fp_ready >> 14;

	//Pitchbend
	int32_t finePB = _synth_pb >> 2;
	if(finePB < 0) {finePB = 0;}
	else if(finePB > 4095) {finePB = 4095;}
	uint16_t lut_pb_low = pitchbend_lut[finePB >> 4];
	uint16_t lut_pb_high = pitchbend_lut[(finePB >> 4) + 1];
	uint16_t pb_ready = lut_pb_low + ((lut_pb_low - lut_pb_high) * (finePB & 0b1111) >> 4);
	//basePitch = basePitch * pb_ready >> 14;

	uint32_t basePitch = 0;
	if (_synth_easter_tuning) {
		basePitch = midi_lut[SYNTH_MIDI_MAX- SYNTH_MIDI_BASE];
		basePitch = basePitch * fp_ready >> 14;
		basePitch = basePitch * pb_ready >> 14;
	}

	// Sound generation

	int32_t so = 0;

	for (uint8_t i = 0; i < NOSC; i++) {
		volatile uint8_t note = _synth_note[i];
		if (note == 0xFF) {continue;}
		if (_synth_vol[i] == 0) {
			if (note != 0xFF) {
				_synth_is_osc_pressed[i] = 0;
				_synth_note_forward_memory[note] = 0xFF;
				_synth_note[i] = 0xFF;
			}
			continue;
		}

		/*
		 *
		uint32_t phinc = basePitch;
		uint8_t revDif = SYNTH_MIDI_MAX - _synth_note[i];
		uint8_t revOct = revDif / 12;
		int8_t octPos = 12 - (revDif % 12);
		if(octPos == 12) {
			octPos = 0;
		} else {
			revOct += 1;
		}

		*/

		// TODO: Quantization
		//volatile int8_t qt_pos = (octPos - 3) % 12;
		//if(qt_pos < 0) {qt_pos += 12;}
		//octPos = arp_scales_quantize_lut[2][qt_pos];
		//octPos += 3;
		//if (octPos >= 12) {octPos -= 12;}

		//phinc >>= (revOct);
		//phinc = (phinc * coarse_lut[octPos + 12]) >> 14;

		uint32_t phinc;

		if (_synth_easter_tuning) {
			phinc = basePitch;
			uint8_t revDif = SYNTH_MIDI_MAX - _synth_note[i];
			uint8_t revOct = revDif / 12;
			int8_t octPos = 12 - (revDif % 12);
			if(octPos == 12) {
				octPos = 0;
			} else {
				revOct += 1;
			}
			phinc >>= (revOct);
			phinc = (phinc * coarse_lut[octPos + 12]) >> 14;
		} else {
			phinc = midi_lut[note - SYNTH_MIDI_BASE];
			phinc = (phinc * fp_ready) >> 14;
			phinc = (phinc * pb_ready) >> 14;
		}



		// Synth engine
		_synth_phase[i] += phinc;
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

		// Decay and add to SO
		v = v * _synth_vol[i];
		if (!_synth_is_osc_pressed[i]
#ifdef MODDED_VERSION
								   || !_synth_drone
#endif
		) {_synth_vol[i] = (_synth_vol[i] * (32767 - decay)) >> 15;}
		so += v;
	}
#ifdef MODDED_VERSION
	so /= 3;
#else
	so /= 6;
#endif
	so >>= (VOLUME_PRECISION);
	so += 512;
	if(so < 0) {so = 0;}
	if(so > 1023) {so = 1023;}
	PWM_AUDIO_OUT = so;
}
