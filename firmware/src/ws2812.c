/*
 * ws2812.c
 * 
 * Copyright 2013 Jon Cross <joncross.cooljc@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

/* ------------------------------------------------------------------ */
/* Include Files ---------------------------------------------------- */
/* ------------------------------------------------------------------ */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>
#include "usb_serial.h"
#include "ws2812.h"

/* --------------------------------------------------------------------
T0H 0 code ,high voltage time 400nS (0.4us) ±150ns
T0L 0 code , low voltage time 850nS (0.85us) ±150ns
+-----+        +
| T0H |  T0L   |
|     +--------+

T1H 1 code ,high voltage time 800nS (0.8us) ±150ns
T1L 1 code ,low voltage time 450nS (0.45us) ±150ns
+--------+     +
|  T1H   | T1L |
|        +-----+

RES low voltage time Above 5000nS (50μs)
|              |
|              |
+--------------+
--------------------------------------------------------------------- */

#define T0_LOW   7 // (875nS)
#define T0_HIGH  3 // (375nS)

#define T1_LOW   3
#define T1_HIGH  7

#define T_RES    40 // (50uS)

typedef enum {
	WS2812_STATE_IDLE = 0,
	WS2812_STATE_SENDING_DATA,
	WS2812_STATE_RESET
} ws2812_states_t;

typedef enum
{
	WS2812_DATA_IDLE = 0,
	WS2812_DATA_START_BIT,
	WS2812_DATA_SENDING,
	WS2812_DATA_NEXT_BIT
} ws2812_data_state_t;

typedef enum {
	WS2812_DATA_BIT_ZERO = 0,
	WS2812_DATA_BIT_ONE
} ws2812_data_bit_states_t;

typedef enum {
	WS2812_T_STATE_LOW = 0,
	WS2812_T_STATE_HIGH
} ws2812_t_state_t;

volatile ws2812_states_t     ws2812_state       = WS2812_STATE_IDLE;
volatile ws2812_t_state_t    ws2812_t_state     = WS2812_T_STATE_HIGH;
volatile ws2812_data_state_t ws2812_data_state  = WS2812_DATA_IDLE;
volatile uint8_t             ws2812_byte_pos    = 0;
volatile uint16_t            ws2812_bit_count   = 0;
volatile uint8_t             ws2812_pulse_count = 0;
volatile uint8_t             ws2812_current_bit = 0;

uint8_t  ws2812_buffer[160];
uint16_t ws2812_data_bit_count_total = 24; //(no_leds * (3 * 8))

void ws2812_send_timer_init(void)
{
	WS2812_GPIO_init();
	// 16000000 = 62.5 nS
	OCR0A  = 2;    // set trigger 125nS including 0
	OCR0B  = 0;
	
	TIMSK0 = 0x02;  // set interrupt mask
	TIFR0  |= 0x02; // clear interrupt if set
	
	TCCR0A = 0x02;  // set mode
	TCCR0B = 0x01;  // 0x82.. set mode and activate timer
}

void ws2812_set_colour (uint8_t leds, uint8_t *buf)
{
	ws2812_data_bit_count_total = (leds * 24);
	ws2812_buffer[0] = buf[0];
	ws2812_buffer[1] = buf[1];
	ws2812_buffer[2] = buf[2];
	ws2812_state = WS2812_STATE_SENDING_DATA;
}

//#define dbg(x) usb_serial_putchar(x)
#define dbg(x)
/* ------------------------------------------------------------------ */
/* trigger every 125nS */
/* ------------------------------------------------------------------ */
//ISR(TIMER0_OVF_vect)
ISR(TIMER0_COMPA_vect)
{
	
	if (ws2812_state == WS2812_STATE_SENDING_DATA) {
		
		if (ws2812_data_state == WS2812_DATA_IDLE)
		{
			ws2812_bit_count   = 0;
			ws2812_pulse_count = 0;
			ws2812_data_state  = WS2812_DATA_START_BIT;
		}
		
		if (ws2812_data_state == WS2812_DATA_START_BIT)
		{
			uint8_t countByte;
			//get first bit and set IR LED to logic high.
			countByte = ws2812_bit_count / 8;
			ws2812_current_bit = ws2812_buffer[countByte] & 0x01;
			
			ws2812_data_state = WS2812_DATA_SENDING;
			dbg('|');
		}
		
		if (ws2812_data_state == WS2812_DATA_SENDING) {
			//ws2812_pulse_count++;
			if (ws2812_current_bit == WS2812_DATA_BIT_ONE) {
				if (ws2812_t_state == WS2812_T_STATE_HIGH) {
					WS2812_pin_on();
					dbg('-');
					if (ws2812_pulse_count == T1_HIGH) {
						// reset pulse count and change T state
						ws2812_t_state = WS2812_T_STATE_LOW;
						ws2812_pulse_count = 0;
					}
				}
				else {
					WS2812_pin_off();
					dbg('_');
					if (ws2812_pulse_count == T1_LOW) {
						// reset pulse count and change T state
						ws2812_t_state = WS2812_T_STATE_HIGH;
						ws2812_pulse_count = 0;
						// change to next bit
						ws2812_bit_count++;
						ws2812_data_state = WS2812_DATA_NEXT_BIT;
					}
				}
			}
			else { /* WS2812_DATA_BIT_ZERO */
				if (ws2812_t_state == WS2812_T_STATE_HIGH) {
					WS2812_pin_on();
					dbg('-');
					if (ws2812_pulse_count == T0_HIGH) {
						// reset pulse count and change T state
						ws2812_t_state = WS2812_T_STATE_LOW;
						ws2812_pulse_count = 0;
					}
				}
				else {
					WS2812_pin_off();
					dbg('_');
					if (ws2812_pulse_count == T0_LOW) {
						// reset pulse count and change T state
						ws2812_t_state = WS2812_T_STATE_HIGH;
						ws2812_pulse_count = 0;
						// change to next bit
						ws2812_bit_count++;
						ws2812_data_state = WS2812_DATA_NEXT_BIT;
					}
				}
			}
			ws2812_pulse_count++;
		} /* WS2812_DATA_SENDING */
		
		if (ws2812_data_state == WS2812_DATA_NEXT_BIT)
		{
			uint8_t countByte;
			uint8_t bitShift;
			//get first bit and set IR LED to logic high.
			countByte = ws2812_bit_count / 8;
			bitShift = ws2812_bit_count % 8;
			
			ws2812_current_bit = ((ws2812_buffer[countByte] >> bitShift) & 0x01);
			
			ws2812_data_state = WS2812_DATA_SENDING;
			if (ws2812_bit_count == ws2812_data_bit_count_total)
			{
				ws2812_data_state = WS2812_DATA_IDLE;
				ws2812_state      = WS2812_STATE_RESET;
			}
			dbg('|');
		} /* WS2812_DATA_NEXT_BIT */
	}
	else if (ws2812_state == WS2812_STATE_RESET) {
		ws2812_pulse_count++;
		WS2812_pin_off();
		if (ws2812_pulse_count == T_RES) {
			ws2812_state = WS2812_STATE_IDLE;
			//WS2812_pin_on();
			usb_serial_putchar('.');
		}
	}
	return;
}

/* EOF */
