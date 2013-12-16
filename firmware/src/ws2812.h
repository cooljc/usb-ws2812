/*
 * ws2812.h
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
#ifndef _WS2812_H
#define _WS2812_H

#define WS2812_PIN				0x01 //b00000100 = bit0

#define WS2812_GPIO_init()		(DDRD |= WS2812_PIN)
#define WS2812_pin_on()			(PORTD |= WS2812_PIN)
#define WS2812_pin_off()		(PORTD &= ~WS2812_PIN)
#define WS2812_pin_toggle()		(PIND |= WS2812_PIN)


void ws2812_send_timer_init(void);
void ws2812_set_colour (uint8_t leds, uint8_t *buf);

#endif /* #ifndef _WS2812_H */
/* EOF */
