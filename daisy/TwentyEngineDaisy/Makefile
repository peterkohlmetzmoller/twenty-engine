# Twenty Engine - Daisy Patch SM
# Build:  make LIBDAISY_DIR=/path/to/libDaisy
# Flash (once): make program-boot   - installs the Daisy bootloader (Patch SM in DFU mode: hold BOOT, tap RESET)
# Flash:        make program-dfu    - tap RESET, then within 2.5 s press BOOT (the LED pulses): the bootloader takes the firmware
TARGET = TwentyEngineDaisy

CPP_SOURCES = TwentyEngineDaisy.cpp ../../engine/engine_globals.cpp
C_SOURCES = usbd_desc_twenty.c

LIBDAISY_DIR ?= ../../../libDaisy
C_INCLUDES += -I../../engine
C_DEFS += -DMS20_RAMFILE -DMS20_OSF=2

OPT = -O2

# runs from SRAM, loaded by the Daisy bootloader from QSPI (the firmware is bigger than the 128 KB internal flash)
APP_TYPE = BOOT_SRAM
LDSCRIPT = TwentyEngineDaisy_sram.lds
CPP_STANDARD = -std=gnu++17

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
