/*
    Hardware watchdog using ATtiny13.

    Copyright (c) Andrew McDonnell, 2015

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */ 

// This was originally developed to kick an ESP8266 when it experienced
// the notorious Fatal Exception (3) on wakeup from deep sleep.
//
// It avoids needing extra watchdog line by just monitoring another signal
// for example, the SPI CS for an external peripheral that is expected to be
// used always after wakeup from deep sleep. If that doesnt happen within
// the expected time, reset the ESP8266.
//
// I suspect that the actual problem is that the Deep Sleep wakeup which pulls
// the ESP8266 Reset line low does not hold the line low for long enough.
// When I observed it using my CRO the deep sleep wake was only about 15ms
// whereas when I reset to recover from the fatal exception I'm generally
// holding the button down a good second.
//
// Thus, this hardware watchdog holds the reset for 500ms
//
// Tested succesfully under real conditions;
// my system which would crash randomly and require physical reset anywhere
// from 10 minutes to several hours, has now been running for several days
// and I have not had to physically reset it.
// I had a serial monitor connected and noticed that in the first 24 hours of use,
// the hardware watchdog had to do its job about 20 times...
//

#define F_CPU (9600000 / 8)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

// Sleep in 100ms intervals until the timeout counter reached, then assert (lower) the reset out line.
// Keep reset at tri state when not lowered. Note that the ports are tri-state on POR.
// When the kicker is asserted - IRQ falling edge on /SX1276_CS - then reset the counter
// On activity, turn on activity LED for a second so we can see it, for debugging 
// On timeout, turn on timeout LED for 15 seconds, for debugging
// In theory the ESP8266 can check that pin soon after boot to see if it was kicked
// First version use delay, then upgrade to timer interrupts to enable sleep mode

// Connecting the LEDs is optional, I used them to debug with

// Programming instructions:

//  avr-gcc -mmcu=attiny13 -Os hwdog.c -std=c99 -o hwdog.elf
//  avr-size --format=avr --mcu=attiny13 hwdog.elf
//  avr-objcopy -j .text -j .data -O ihex hwdog.elf hwdog.hex
//  sudo avrdude -c usbtiny  -v -p attiny13 -U flash:w:hwdog.hex:i


#define TICK_MS 100
#define TIMEOUT_MINUTES 3
#define TIMEOUT_TICKS ((uint32_t)TIMEOUT_MINUTES * (uint32_t)60 * (uint32_t)1000 / (uint32_t)TICK_MS)

#define TIMEOUT_LED_MS 15100u   // Max value: 25400 + 100
#define ACTIVITY_LED_MS 200u   // Max value: 25400 + 100 (add 10 for exact)

#define LED_ACTV PB3   // ATiny13 PDIP phys pin 2
#define PIN_KICK PB4   // ATiny13 PDIP phys pin 3, PCINT4
#define LED_TIME PB0   // ATiny13 PDIP phys pin 5
#define PIN_RSTO PB1   // ATiny13 PDIP phys pin 6

volatile bool activity_flag = false;

// When kick interrupt happens, reset the timeout counter
ISR(PCINT0_vect)
{
  activity_flag = true;
}

#define FLASH(x)   DDRB |= _BV(x);  for (int8_t i=0; i < 4; i++) { PORTB |= _BV(x); _delay_ms(330); PORTB &= ~ _BV(x); _delay_ms(330); } DDRB &= ~_BV(x);

#if 1
#define FLASH_TEST(x) FLASH(x)
#else
#define FLASH_TEST(x) do { } while (0)
#endif

int main(void)
{
  uint32_t timeout_counter = 0;
  uint8_t timeout_led_count = 0;
  uint8_t activity_led_count = 0;
  
  // Enable both LED for 3 seconds at power on, just because I can
  DDRB |= _BV(LED_ACTV) | _BV(LED_TIME);
  PORTB |= _BV(LED_ACTV) | _BV(LED_TIME);
  _delay_ms(3000);

  // Turn off both LED
  PORTB &= ~ (_BV(LED_ACTV) | _BV(LED_TIME));

  // Enable pin change interrupts
  GIMSK |= 1 << PCIE;

  // Turn on interrupt on PB4
  PCMSK = 1 << PCINT4;

  // enable interrupts
  sei();

  while (1) {
    _delay_ms(100);
    if (activity_flag) {
      // if we got any interrupts in past 100ms, then reset the counter
      activity_flag = false;
      timeout_counter = 0;
      activity_led_count = (ACTIVITY_LED_MS / TICK_MS);
      DDRB |= _BV(LED_ACTV);
      PORTB |= _BV(LED_ACTV);
    }
    else if (timeout_counter ++ >= TIMEOUT_TICKS) {
      // Flash the debug LED on timeout, just because I felt like it
      FLASH_TEST(PB3);
      // Turn on timeout LED
      PORTB |= _BV(LED_TIME);
      DDRB |= _BV(LED_TIME);
      // we do this using a delay; which means that the timeout is actually for TIMEOUT_TICKS beyond the reset pulse, not including it
      DDRB |= _BV(PIN_RSTO);
      // nice long one, like the user pressing the reset button
      _delay_ms(500);
      DDRB &= ~_BV(PIN_RSTO); // back to tri state
      timeout_led_count = (TIMEOUT_LED_MS / TICK_MS);
      timeout_counter = 0;
    }
    if (timeout_led_count > 0) { // this will be one tick less than expected because we are checking immediately.
      timeout_led_count --;
      // Turn off timeout LED after sufficient period
      if (!timeout_led_count) {
        PORTB &= ~_BV(LED_TIME);
      }
    }
    if (activity_led_count > 0) {
      activity_led_count --;
      // Turn off timeout LED after sufficient period
      if (!activity_led_count) {
        PORTB &= ~_BV(LED_ACTV);
      }
    }
  }
}
 
