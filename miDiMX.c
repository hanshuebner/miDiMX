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

/* Function Prototypes: */
void SetupHardware(void);

#define TXBITMASK 0x08

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

  DDRD |= TXBITMASK;

  /* Hardware Initialization */
  LEDs_Init();
  USB_Init();
}

#define WAIT_FOR_UART_TX_EMPTY while (!(UCSR1A & (1 << UDRE1)))
#define WAIT_FOR_UART_TX_COMPLETE while (!(UCSR1A & (1 << TXC1)))

void sendDmxFrame(uint8_t* data, uint16_t len)
{
  LEDs_SetAllLEDs(LEDS_LED1);
  PORTD &= ~TXBITMASK;
  _delay_us(88);
  PORTD |= TXBITMASK;
  _delay_us(8);

  UCSR1B |= (1 << TXEN1);
  WAIT_FOR_UART_TX_EMPTY;
  UDR1 = 0;                                                 /* first byte is always zero for now */
  WAIT_FOR_UART_TX_COMPLETE;
  for (int i = 0; i < len; i++) {
    WAIT_FOR_UART_TX_EMPTY;
    UDR1 = data[i];
    WAIT_FOR_UART_TX_COMPLETE;
  }
  UCSR1B &= ~(1 << TXEN1);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
}

int main(void) {

  SetupHardware();

  LEDs_SetAllLEDs(LEDS_LED1);
  _delay_ms(100);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
  _delay_ms(100);
  LEDs_SetAllLEDs(LEDS_LED1);
  _delay_ms(100);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
  _delay_ms(100);
  LEDs_SetAllLEDs(LEDS_LED1);
  _delay_ms(100);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
  _delay_ms(100);

  sei();

  static uint8_t data[128];
  static uint8_t channels = 0;

  for (;;) {
    MIDI_EventPacket_t ReceivedMIDIEvent;
    while (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent)) {
      uint8_t command = ReceivedMIDIEvent.Data1 & 0xf0;
      switch (command) {
      case MIDI_COMMAND_NOTE_ON:
      case MIDI_COMMAND_NOTE_OFF:
        {
          uint8_t note = ReceivedMIDIEvent.Data2;
          uint8_t velocity = (command == MIDI_COMMAND_NOTE_ON) ? ReceivedMIDIEvent.Data3 : 0;
          data[note] = velocity << 1;
          if (note > channels) {
            channels = note;
          }
          sendDmxFrame(data, channels + 1);
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
  bool ConfigSuccess = true;

  ConfigSuccess &= MIDI_Device_ConfigureEndpoints(&Keyboard_MIDI_Interface);
}

