# Twenty Engine – Teensy 4.1 edition

Teensy 4.1 + SGTL5000 audio board (PJRC Rev D pinout) + MS-20 iC on the USB host port.

Open `TwentyEngineTeensy/TwentyEngineTeensy.ino` in the Arduino IDE with Teensyduino:
- Tools > Board > Teensy 4.1
- Tools > USB Type > **Serial + MIDI**
- Tools > CPU Speed > 600 MHz

`engine_gen.h`, `ksoloti_host.h` and `engine_globals.cpp` are copies of the files in `engine/`
(the Arduino IDE only compiles files inside the sketch folder). After regenerating the engine, copy them again.

The patch memory is saved in the Teensy's own flash (no SD card needed). An SD card (FAT32) in the Teensy's slot, if present, gets a backup copy (`MS20MEM.BIN`); with nothing in flash yet, a card's file (e.g. from a Ksoloti) is taken over.
