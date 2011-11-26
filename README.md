# miDiMX - LUFA based USB MIDI to DMX controller #

This program implements an USB
[MIDI](http://en.wikipedia.org/wiki/MIDI) to
[DMX](http://en.wikipedia.org/wiki/DMX512) interface using the
[LUFA](http://www.fourwalledcubicle.com/LUFA.php) library.  I am using
it with the awesome [Teensy](http://www.pjrc.com/teensy/) hardware.

MIDI "Note On" and "Note Off" messages are received on channel 0.  The
pitch is mapped to the DMX channel number (0-127).  For "Note On"
messages, the velocity is multiplied by two to yield the value set on
that channel.  "Note Off" messages set the corresponding channel to
zero (off).

Questions/comments?  Send email: hans.huebner@gmail.com