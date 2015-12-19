# hardware_watchdog
Hardware watchdog for ESP8266 made using an ATtiny13

## Background

This program was originally developed to kick an ESP8266 when it experienced
the notorious Fatal Exception (3) on wakeup from deep sleep.

It avoids needing an extra watchdog line by just monitoring another signal:
for example, the SPI CS for an external peripheral always expected to be
used after wakeup from deep sleep. If there is no activity within the
expected time, reset the ESP8266.

I suspect that the actual problem is that the Deep Sleep wakeup which pulls
the ESP8266 Reset line low does not hold the line low for long enough.
On observing the wakeup using my CRO I noticed that the deep sleep wake
only held reset low for about 15ms.

I was always able to recover from the Fatal Exception by pressing the reset button.
Therefore I designed this hardware watchdog to hold reset for 500ms.

## Configuration

Change `TIMEOUT_MINUTES` to the desired value.

## Circuit

* Connect 3V3 and GND to the ATtiny13
* Connect the signal that indicates proper operation to pin 3 on the ATtiny13
  * I'm using a pin change interrupt which keeps things simple
* Connect the ESP8266 RESET line to pin 6 of the ATtiny13
* The watchdog will also drive LEDs on pins 2 and 5 for debugging:
  * Pin 2: latches for 200ms whenever a pin change interrupt occurs
  * Pin 5: blinks then stays on for 15 seconds whenever the ESP8266 reset is activated by us

## Flashing

Instructions for flashing the ATtiny13 using a USB ISP via Linux are in the source code.

## Testing

Tested succesfully under real conditions;
my system which would crash randomly and require physical reset anywhere
from 10 minutes to several hours.  Since building the watchgod, it has now
been running for several days and I have not had to physically reset it.

I had a serial monitor connected and noticed that in the first 24 hours of use,
the hardware watchdog had to do its job about 20 times...

