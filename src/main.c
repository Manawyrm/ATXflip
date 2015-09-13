/*
@brief               soft-powerswitching for ATX power supply
@date                2013-08-24
@version             1.50
@author              Tobias Mädel
@copyright           (C) 2013-2015 Tobias Mädel (t.maedel@alfeld.de / https://tbspace.de)
@license             MIT

@par 2015-09-08 Tobias Mädel Added turbo switching and ATX behavior
@par 2013-08-24 Tobias Mädel Initial version
*/

// fuses: -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
#define F_CPU 1000000UL

#define PWRBTN 4 // power-on switch pin
#define TURBO  3 // turbo at-mb pin
#define PSON   2 // power-on atx-supply pin

#define JMP_ATXBEHAV 1 // x86 protected mode behaviour (hold 5 seconds to power down) jumper pin
#define JMP_LASTSTATE 0 // remember last power state in EEPROM

#define ATX_TURBOTIME 1
#define ATX_POWERDOWNTIME 5

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>

int main(void)
{
	// enable internal pull-up on the power button pin
	bitSet  (PORTB, PWRBTN);

	// enable internal pull-up for jumper pins
	bitSet  (PORTB, JMP_ATXBEHAV);
	bitSet  (PORTB, JMP_LASTSTATE);

	// read jumper settings
	uint8_t mode = bitRead(PINB, JMP_ATXBEHAV);
	uint8_t lastStateSave = bitRead(PINB, JMP_LASTSTATE);

	// initial setup delay. prevents turning the power supply on while its still initializing
	_delay_ms(100);

	uint8_t state = 0;
	// if EEPROM-laststate saving is enabled, 
	if (lastStateSave)
	{
		uint8_t turboState = eeprom_read_byte(2);
		bitWrite(DDRB, TURBO, turboState);

		// read eeprom data
		state = eeprom_read_byte(1);
		// if last state was ON then enable ATX psu
		if (state != 0)
		{
			bitWrite(DDRB, PSON, 1);
		}
	}

	// waiting...
	while(1)
	{

		if (state == 0)
		{
			// wait for falling edge
			while (bitRead(PINB, PWRBTN)) {_delay_ms(1);}

			state = 1;
			bitWrite(DDRB, PSON, 1);
			eeprom_write_byte(1, state); 

			// debounce
			_delay_ms(100);

			// wait for rising edge (releasing button)
			while (!bitRead(PINB, PWRBTN)) {_delay_ms(1);}
		}


		if (state != 0)
		{
			// wait for falling edge
			while (bitRead(PINB, PWRBTN)) {_delay_ms(1);}

			if (mode == 0)
			{
				state = 0;
				bitWrite(DDRB, PSON, 0);
				eeprom_write_byte(1, state); 
			}

			// debounce
			_delay_ms(10);

			// wait for rising edge (releasing button)
			uint16_t holdTime = 0;
			while (!bitRead(PINB, PWRBTN))
			{
				if (mode != 0)
				{
					// atx behaviour mode
					holdTime++;
					// who holds buttons down that long? Anyway: reset time to prevent overflowing 
					if (holdTime == 65530) holdTime = 0; 

					// if the button is held down for the set time, turn the supply off
					if (holdTime > ATX_POWERDOWNTIME * 100)
					{
						state = 0;
						bitWrite(DDRB, PSON, 0);
						eeprom_write_byte(1, state); 
					}

					_delay_ms(10);
				}
				else
				{
					// at behaviour mode
					_delay_ms(10);
				}
			}

			// if in atx mode, check time held before releasing
			if (mode != 0)
			{
				if (holdTime < (ATX_POWERDOWNTIME * 100) && holdTime > (ATX_TURBOTIME * 100))
				{
					uint8_t turboState = eeprom_read_byte(2);
					turboState = !turboState;
					bitWrite(DDRB, TURBO, turboState);
					eeprom_write_byte(2, turboState); 
				}
			}
		}


	}
}
