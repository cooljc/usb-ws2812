/*
 * USB-WS2812 Gadget
 * This firmware provides a method of controlling WS2812 devices from USB.
 * All code related to IR codes is Copyright (c) 2012 Panasonic
 *
 * The software is based on:
 * Simple example for Teensy USB Development Board
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2008 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include "usb_serial.h"
#include "ws2812.h"

#define LED_CONFIG	(DDRD |= (1<<6))
#define LED_ON		(PORTD |= (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))

/* ------------------------------------------------------------------ */
/* Global Variables */
/* ------------------------------------------------------------------ */
char ascii[] = {'0', '1', '2', '3', '4', '5', '6', '7',
				'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

/* ------------------------------------------------------------------ */
/* function prototypes */
/* ------------------------------------------------------------------ */
void     usbws2812_showHelp (void);
void     usbws2812_sendStr (const char *s);
void     usbws2812_sendAsciiByte (uint8_t byte);
void     usbws2812_sendAsciiWord (uint16_t word);
void     usbws2812_sendNewLine (void);
uint8_t  usbws2812_ascii2Byte (const char *data);
uint16_t usbws2812_ascii2Word (const char *data);
uint8_t  usbws2812_recvStr (char *buf, uint8_t size);
void     usbws2812_parseAndExecuteCommand (const char *buf, uint8_t num);



/* ------------------------------------------------------------------ */
/* Basic command interpreter for sending and receiving IR commands    */
/* ------------------------------------------------------------------ */
int main(void)
{
	char buf[32];
	uint8_t n;

	// set for 16 MHz clock, and turn on the LED
	CPU_PRESCALE(0);
	LED_CONFIG;
	LED_ON;

	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this
	// will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;
	_delay_ms(1000);

	ws2812_send_timer_init();
	
	while (1) {
		// wait for the user to run their terminal emulator program
		// which sets DTR to indicate it is ready to receive.
		while (!(usb_serial_get_control() & USB_SERIAL_DTR)) /* wait */ ;

		// discard anything that was received prior.  Sometimes the
		// operating system or other software will send a modem
		// "AT command", which can still be buffered.
		usb_serial_flush_input();

		// print a nice welcome message
		usbws2812_showHelp();

		LED_OFF;

		// and then listen for commands and process them
		while (1) {
			//usbws2812_sendStr(PSTR("# "));
			n = usbws2812_recvStr(buf, sizeof(buf));
			if (n == 255) break;
			//usbws2812_sendStr(PSTR("\r\n"));
			usbws2812_parseAndExecuteCommand(buf, n);
		}

		LED_ON;
	}
}

/* ------------------------------------------------------------------ */
/* show help */
/* ------------------------------------------------------------------ */
void usbws2812_showHelp(void)
{
	usbws2812_sendStr(PSTR("\r\nUSB-WS2812 Gadget, "
			"Sends colour bytes to WS2812 chips\r\n\r\n"
			"Example Commands\r\n"
			"  send <led count> <RGB1> <RGB2> <RGBn> - Send a command\r\n"
			"  help                         - Display this screen\r\n"));
}

/* ------------------------------------------------------------------ */
/* Send a string to the USB serial port.  The string must be in
 * flash memory, using PSTR */
/* ------------------------------------------------------------------ */
void usbws2812_sendStr(const char *s)
{
	char c;
	while (1) {
		c = pgm_read_byte(s++);
		if (!c) break;
		usb_serial_putchar(c);
	}
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
void usbws2812_sendAsciiByte (uint8_t byte)
{
	usb_serial_putchar (ascii[((byte >> 4) & 0x0f)]);
	usb_serial_putchar (ascii[(byte & 0x0f)]);
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
void usbws2812_sendAsciiWord (uint16_t word)
{
	usbws2812_sendAsciiByte ( (uint8_t)((word >> 8) & 0xff) );
	usbws2812_sendAsciiByte ( (uint8_t)((word) & 0xff) );
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
void usbws2812_sendNewLine (void)
{
	usbws2812_sendStr(PSTR("\r\n"));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
uint8_t usbws2812_ascii2Byte (const char *data)
{
	uint8_t nibble = 0;
	uint8_t value = 0;
	uint8_t pos = 0;
	for (nibble = 0; nibble<2; nibble++) {
		value = (value << 4);
		for (pos=0; pos<16; pos++) {
			if (ascii[pos] == data[nibble]) {
				value |= pos;
				break;
			}
		}
	}
	return value;
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
uint16_t usbws2812_ascii2Word (const char *data)
{
	uint8_t nibble = 0;
	uint16_t value = 0;
	uint8_t pos = 0;
	for (nibble = 0; nibble<4; nibble++) {
		value = (value << 4);
		for (pos=0; pos<16; pos++) {
			if (ascii[pos] == data[nibble]) {
				value |= pos;
				break;
			}
		}
	}
	return value;
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Receive a string from the USB serial port.  The string is stored
 * in the buffer and this function will not exceed the buffer size.
 * A carriage return or newline completes the string, and is not
 * stored into the buffer.
 * The return value is the number of characters received, or 255 if
 * the virtual serial connection was closed while waiting. */
/* ------------------------------------------------------------------ */
uint8_t usbws2812_recvStr(char *buf, uint8_t size)
{
	int16_t r;
	uint8_t count=0;

	while (count < size) {
		r = usb_serial_getchar();
		if (r != -1) {
			if (r == '\r' || r == '\n') return count;
			/* handle back space */
			if (r == 0x08) {
				if (count > 0) {
					/* set buffer to zero. not needed but compiler
					 * gives a warning if we don't do it.*/
					*buf-- = 0;
					count--;
					/* remove previous character and set cursor one
					 * character back */
					usb_serial_putchar(0x08);
					usb_serial_putchar(' ');
					usb_serial_putchar(0x08);
				}
			}
			else if (r >= ' ' && r <= '~') {
				*buf++ = r;
				usb_serial_putchar(r);
				count++;
			}
		} else {
			if (!usb_configured() ||
			  !(usb_serial_get_control() & USB_SERIAL_DTR)) {
				// user no longer connected
				return 255;
			}
			// just a normal timeout, keep waiting
		}
	}
	return count;
}

/* ------------------------------------------------------------------ */
/* parse a user command and execute it, or print an error message */
/* ------------------------------------------------------------------ */
void usbws2812_parseAndExecuteCommand(const char *buf, uint8_t num)
{
	uint8_t command_valid = 0;

	if (num >= 4) {
		if (strncmp(buf, "send", 4) == 0) {
			uint8_t  buf[3] ={0xff,0,0};

			/* set command valid flag */
			command_valid = 1;
			
			ws2812_set_colour(1, buf);

			/* check number of bytes is enough to parse command */
			if (num != 15) {
				usbws2812_sendStr(PSTR("send: invalid options!\r\n"));
				usbws2812_sendStr(PSTR("usage: send <led count> <RGB1> <RGB2> <RGBn>\r\n"));
				usbws2812_sendStr(PSTR("eg:    send 3 FF0000 00FF00 0000FF\r\n"));
				return;
			}
			/* TODO: parse command args */
			/* TODO: end command to ws2812 */
			
		}
		else if (strncmp(buf, "help", 4) == 0) {
			/* set command valid flag */
			command_valid = 1;
			/* display help screen */
			usbws2812_showHelp();
		}
		else if (strncmp(buf, "status", 5) == 0) {
			usbws2812_sendStr(PSTR("Status: "));
			usbws2812_sendAsciiByte(0);
			usbws2812_sendNewLine();
			/* set command valid flag */
			command_valid = 1;
		}
	}

	if (command_valid == 0) {
		//usbws2812_sendStr(PSTR("Unknown Command!!\r\n"));
		ws2812_set_colour(1, buf);
	}
}


