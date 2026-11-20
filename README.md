# sxlab-chord-v15
# SxLab Chord v1.5

SxLab Chord v1.5 synth schematics and firmware

Chord is a digital synthesizer made for noise, drone and ambient genres. Its 6 oscillator pitches can be easily tuned to different chords and intervals, with 6 LFOs modulating the settings. Thanks to its vast modulation abilities and rich MIDI implementation, it will be useful in many setups, situations and genres.

Chord v1.5 is a firmware (not hardware) update, that adds Chord Winter Edition synth engines, a few minor MIDI implementation improvements and a bootloader that allows firmware updating through MIDI and DTMF audio.

Chord v1.5 bootloader uses a web updater, that can be found here: https://files.sxlab.ru/chord-updater/

https://sxlab.ru/chordv15

All fimware is compiled with Stm32CubeIDE 1.6.1 with -Ofast

![SxLab Chord](chord-image.png?raw=true "SxLab Chord v1.5")

### Files

- *Schematics* - SxLab Chord schematics
    - **Chord top PCB Sch.pdf** - Top board schematic (uC, pots, buttons, etc.)
	- **Chord bottom PCB Sch.pdf** - Bottom board schematic (power, output filter/amp, MIDI input)
- **Compiled/chord-v1.5-bootloader.hex** - Compiled bootloader firmware
- *FW-Bootloader* - SxLab Chord v1.5 bootloader source code
- **Compiled/chord-v1.5-fw1.0.hex** - Compiled firmware
- *FW-Main* - SxLab Chord v1.5 main firmware source code, main files:
	- **Inc/main.h** - Common includes
	- **Inc/main.c** - Main firmware C file
	- **Inc/pwm.h**, **Src/pwm.c** - Main (TIM1) interrupt initialization, PWM initialization (TIM1, TIM3) for knob LEDs and sound output
	- **Inc/tick.h**, **Src/tick.c** - Secondary (TIM17) interrupt (tick), updates and interface
	- **Inc/synth.h**, **Src/synth.c** - Main synth code, preset loading (synth engines implemented separately)
	- **Inc/synth_lut.h**, **Src/synth_lut.c** - Synth code LUTs
	- **Inc/lfo.h**, **Inc/lfo.c** - LFO code
	- **Inc/lfo_sync.c** - LFO MIDI sync implementation, using TIM16 & TIM14 interrupts
    - **Inc/adc.h**, **Src/adc.c** - ADC Init and interrupt, writing via DMA to *adcData[]*
	- **Inc/butled.h**, **Src/butled.c** - Buttons and their respective LEDs, *but_update()* called in tick.c
	- **Inc/clock.h**, **Src/clock.c** - RCC configuration and SysTick delay function
	- **Inc/eeprom.h**, **Src/eeprom.c** - EEPROM I2C connection, EEPROM memory map
	- **Inc/i2c_common.h**, **Inc/i2c.h**, **Src/i2c.c** - I2C initialization, communication (for EEPROM)	
	- **Inc/midi.h**, **Src/midi.c** - MIDI parsing and receiving using USART1
	- **Inc/engines/s_chord.h**, **Src/engines/s_chord.c** - Chord v1.0 synth engine
	- **Inc/engines/s_arp_chord.h**, **Src/engines/s_arp_chord.c** - CWE chord arp engine
	- **Inc/engines/s_arp.h**, **Src/engines/s_arp.c** - CWE scale arp engine (6-voice MIDI polyphony)