#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>

#include "Descriptors.h"

#include <LUFA/Version.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/USB/USB.h>

/** LUFA MIDI Class driver interface configuration and state information. This structure is
 *  passed to all MIDI Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_MIDI_Device_t Keyboard_MIDI_Interface = {
  .Config = {
    .StreamingInterfaceNumber = 1,

    .DataINEndpointNumber      = MIDI_STREAM_IN_EPNUM,
    .DataINEndpointSize        = MIDI_STREAM_EPSIZE,
    .DataINEndpointDoubleBank  = false,

    .DataOUTEndpointNumber     = MIDI_STREAM_OUT_EPNUM,
    .DataOUTEndpointSize       = MIDI_STREAM_EPSIZE,
    .DataOUTEndpointDoubleBank = false,
  },
};

static inline void
blinkLed(uint16_t onTime, uint16_t offTime)
{
  LEDs_SetAllLEDs(LEDS_LED1);
  _delay_ms(onTime);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
  _delay_ms(offTime);
}

void
error(uint8_t code)
{
  static const uint8_t blinkOnTime = 50;
  static const uint8_t blinkOffTime = 200;
  static const uint16_t errorCycleTime = 2000;

  while (true) {
    for (uint8_t i = 0; i < code; i++) {
      blinkLed(blinkOnTime, blinkOffTime);
    }
    _delay_ms(errorCycleTime - (code * (blinkOnTime + blinkOffTime)));
  }
}

void SetupHardware(void) {
  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable clock division */
  clock_prescale_set(clock_div_1);

  /* Initialize UART */
  UBRR1L = F_CPU/4000000 - 1; // 250kbps
  UBRR1H =  0;
  UCSR1A =  0; // clear error flags
  UCSR1C =  (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10); // 8 data bits, 2 stop bits, no parity (8N2)
  UCSR1B =  0; // turn off for now.

  DDRD |= (1 << PD3);                                       /* PD3 is UART TX line, used as output */

  /* Hardware Initialization */
  LEDs_Init();
  USB_Init();
}

static inline void
sendByte(uint8_t c)
{
  while (!(UCSR1A & (1 << UDRE1)))
    ;                                                       /* Wait for transmitter empty */
  UDR1 = c;
  while (!(UCSR1A & (1 << TXC1)))
    ;                                                       /* Wait for transmission complete */
}

void sendDmxFrame(uint8_t* data, uint16_t len)
{
  LEDs_SetAllLEDs(LEDS_LED1);

  PORTD &= ~(1 << PD3);                                     /* transmit MAB to indicate start of frame */
  _delay_us(92);                                            /* use liberal 92/12 usec frame start sequence */
  PORTD |= (1 << PD3);
  _delay_us(12);

  UCSR1B |= (1 << TXEN1);                                   /* switch transmitter on */
  sendByte(0);                                              /* send frame start code */
  for (int i = 0; i < len; i++) {
    sendByte(data[i]);
  }
  UCSR1B &= ~(1 << TXEN1);                                  /* switch transmitter off */

  LEDs_SetAllLEDs(LEDS_NO_LEDS);
}

int main(void) {

  SetupHardware();

  blinkLed(100, 100);
  blinkLed(100, 100);
  blinkLed(100, 100);

  sei();

  static uint8_t data[128];
  static uint8_t maxChannel = 0;

  for (;;) {
    MIDI_EventPacket_t ReceivedMIDIEvent;
    while (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent)) {
      uint8_t command = ReceivedMIDIEvent.Data1 & 0xf0;
      switch (command) {
      case MIDI_COMMAND_NOTE_ON:
      case MIDI_COMMAND_NOTE_OFF:
        {
          uint8_t channel = ReceivedMIDIEvent.Data2;
          uint8_t brightness = (command == MIDI_COMMAND_NOTE_ON) ? (ReceivedMIDIEvent.Data3 << 2) : 0;
          data[channel] = brightness;
          if (channel > maxChannel) {
            maxChannel = channel;
          }
          sendDmxFrame(data, maxChannel + 1);
        }
      }
    }
    MIDI_Device_USBTask(&Keyboard_MIDI_Interface);
    USB_USBTask();
  }
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
  if (!MIDI_Device_ConfigureEndpoints(&Keyboard_MIDI_Interface)) {
    error(1);
  }
}

