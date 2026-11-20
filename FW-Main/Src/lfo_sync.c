/*
 * lfo_sync.c
 *
 *  Created on: 27 сент. 2023 г.
 *      Author: Sx107
 */


#include <lfo.h>

// Стандарт MIDI: 24 ppqn

// По ТЗ: синхра от период = 1/32 до период = 4 такта
// Для референса: 1 bpm,  время между тиками = 41666мкс
// Для референса: 20 bpm,  время между тиками = 2083мкс
// Для референса: 60 bpm,  время между тиками = 694мкс
// Для референса: 120 bpm, время между тиками = 347мкс
// Для референса: 300 bpm, время между тиками = 138мкс
// Для референса: 999 bpm, время между тиками = 41мкс

volatile uint8_t _lfo_sync_running = 0;
volatile uint32_t _lfo_sync_qticks = 0; // Количество 24-тиков, НО оно обнуляется каждую 1/32, т.е. 0-2
volatile uint32_t _lfo_sync_mticks = 0; // Количество 1/32, НО оно обнуляется каждые 4 такта, т.е. 0-(32*4) = 0-128
volatile uint32_t _lfo_sync_subticks = 0; // subticks - то, что в итоге идет в LFO. Измеряется в четвертях, 1/32 = 1<<17, 1/4 = 1 << 20
volatile uint16_t _lfo_sync_measspd = 0; // Хранит последнее измеренное время между тиками
volatile uint16_t _lfo_sync_last_spd = 0; // Хранит последнее измеренное время между тиками для того чтобы с него начать потом

#ifdef LFO_SYNC_DEBUG
	volatile uint8_t midi_buf[256];
	volatile uint8_t midi_buf_pos = 0;

	static inline void lfo_syncdebug() {
		static volatile uint16_t tdiff = 0;
		midi_buf[midi_buf_pos++] = (lfo_tick-tdiff) & 0xFF;
		tdiff = lfo_tick;
	}
#endif

void lfo_sync_configure() {

	RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
	RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

	// Tim16, после PSC имеем 48000000 / 128 = 375кГц
	// Измеряет время между тиками
	// Разрешение: 2,(6) мкс
	// Максимальное время: 174 мс
	TIM16->PSC = 127;
	TIM16->ARR = 65535;
	//TIM16->DIER |= TIM_DIER_UIE;
	TIM16->CR1 |= TIM_CR1_ARPE | TIM_CR1_OPM;
	__DSB();
	TIM16->CR1 |= TIM_CR1_CEN;

	// Tim14, частота после PSC в 16 раз больше, 3 МГц ровно
	// Разрешение: 0.(3) мкс
	// Период тут выходит от 0.(3)мкс до 21.845мс
	NVIC_SetPriority(TIM14_IRQn, 1); // Приоритет UART если что тоже 1! TODO: Тут мб 0 надо?
	NVIC_EnableIRQ(TIM14_IRQn);

	TIM14->PSC = 15;
	TIM14->ARR = 100;
	TIM14->DIER |= TIM_DIER_UIE;
	TIM14->CR1 |= TIM_CR1_ARPE;
	__DSB();
	//TIM14->CR1 |= TIM_CR1_CEN;
}

void lfo_sync_start() {
	_lfo_sync_running = 1;
	_lfo_sync_mticks = 0;
	_lfo_sync_qticks = 0;
	_lfo_sync_subticks = 0;
	_lfo_sync_measspd = _lfo_sync_last_spd;
	TIM14->ARR = _lfo_sync_measspd;
	TIM14->EGR |= TIM_EGR_UG;
	//TIM14->CR1 |= TIM_CR1_CEN;

	#ifdef LFO_SYNC_DEBUG
		midi_buf[midi_buf_pos++] = 'S';
		lfo_syncdebug();
	#endif
}
void lfo_sync_stop() {
	_lfo_sync_running = 0;
	TIM16->CR1 &= ~TIM_CR1_CEN;
	TIM14->CR1 &= ~TIM_CR1_CEN;
	TIM16->CNT = 0;
	TIM16->EGR |= TIM_EGR_UG;
	#ifdef LFO_SYNC_DEBUG
		midi_buf[midi_buf_pos] = 'E';
		lfo_syncdebug();
	#endif
	//midi_buf_pos = 0;
}
void lfo_sync_continue() {
	_lfo_sync_running = 1;
	#ifdef LFO_SYNC_DEBUG
		midi_buf[midi_buf_pos++] = 'C';
		lfo_syncdebug();
	#endif
}
void lfo_sync_tick() {
	volatile uint16_t spd = TIM16->CNT;
	__DSB();
	TIM16->EGR |= TIM_EGR_UG;
	TIM16->CNT = 0;
	TIM16->CR1 |= TIM_CR1_CEN;
	if(_lfo_sync_running && spd != 0 && spd != 65535) {
		TIM14->ARR = _lfo_sync_measspd; // То есть, измерение TIM16 копируется во время TIM14! И частота умножается на 16 так.
		_lfo_sync_measspd = spd;
		_lfo_sync_last_spd = spd;
		TIM14->CR1 |= TIM_CR1_CEN;
	}

	// 1/32 нота = 1 << 17 = 131072, поэтому _lfo_sync_mticks << 17
	// Теперь qticks. За 3 тика они должны накапливать 1/32 ноту
	// 131072 / 3 = 43690,3
	// Округлять надо ВНИЗ! Чтобы грамотно работали 1/32 триоли. Это ппц
	_lfo_sync_subticks = _lfo_sync_qticks * 43691 + (_lfo_sync_mticks << 17);

	_lfo_sync_qticks += 1;
	if(_lfo_sync_qticks >= 3) {_lfo_sync_qticks = 0; _lfo_sync_mticks += 1; }
	if(_lfo_sync_mticks >= 128) {_lfo_sync_mticks = 0;}


	TIM14->DIER &= ~TIM_DIER_UIE;
	TIM14->EGR |= TIM_EGR_UG; // Так, не пон? Да, оно сбросит все, но разве проблемы не будет с двойным срабатыванием????
	TIM14->DIER |= TIM_DIER_UIE;

	#ifdef LFO_SYNC_DEBUG
		midi_buf[midi_buf_pos++] = 'T';
		lfo_syncdebug();
	#endif
}

uint16_t lfo_sync_value(uint8_t lfo_spd) {
	return (_lfo_sync_subticks >> (lfo_spd + 3)) & 0x1FFF;
	// 64th note = 0
	// 32nd note = 1
	// 16th note =   2
	// 8th note = 3
	// 1/4 note = 16 - 12 = 4
	// 1/2 note = 5
	// 1 measure = 6
	// 2 measures = 7
	// 4 measures = 8
}

void lfo_sync_setpos(uint16_t pos) {
	//midi_sync_pos = (pos * 6) % 24;

	// pos = позиция в 1/16? А поточнее никак? Ну да ладно
	// То есть Одно pos = 6 MIDI клоков.
	// То есть, qticks = 0
	_lfo_sync_qticks = 0;

	// Одно pos = 1/16, 1/32 = 2 * 1/16
	_lfo_sync_mticks = ((uint32_t)pos * 2) & 0x7F;

	_lfo_sync_subticks = _lfo_sync_qticks * 43691 + (_lfo_sync_mticks << 17); // TODO: если обновлять точность, то здесь тоже!

	#ifdef LFO_SYNC_DEBUG
		midi_buf[midi_buf_pos++] = 'P';
		lfo_syncdebug();
		midi_buf[midi_buf_pos++] = pos;
	#endif
}

void TIM14_IRQHandler() {
	// Срабатывает TIM14 ТИПА в 16 раз быстрее тиков MIDI
	// То есть, 24*16=384 ppqn, на 1/32 ноту выходит 48 сабтиков?
	// Еще раз: в subticks 1/32 нота = 131072
	// 131072 / 48 = 2730,(6)
	// Но почему-то частота на самом деле только в 8 раз ниже. Возможно, особенности таймеров.
	// 131072 / 24 = 2730,(6)
	_lfo_sync_subticks += 5461;

	//_lfo_sync_subticks += 341;
	TIM14->SR &= ~TIM_SR_UIF;
}
