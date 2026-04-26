I. Building binaries from source with ARM GCC compiler

The firmware consist of 2 parts: 
- bootloader 
- application

Both parts can be build separately or together. For building use following commands:

1) Building bootloader:	

> make boot

2) Building application: 

> make app

3) Building both bootloader and application:

> make 

or 

> make all

4) Cleaning build directories:

> make clean


After building, binaries and hex files are placed in per-target build
sub-directories:

/build/<target>/app/FreeJoy.bin
/build/<target>/app/FreeJoy.hex
/build/<target>/boot/Bootloader.bin

The chip family is selected via the TARGET=<name> variable, defaulting to
f103. The chip-specific flags / driver source lists / linker scripts /
startup file live in target_<name>.mk; the makefiles themselves stay
target-agnostic.

5) Selecting the target chip family:

> make                         (defaults to TARGET=f103, output under build/f103/...)

> make TARGET=f103             (explicit; same result as above)

> make TARGET=f411             (planned for the F411 BlackPill port; stubbed
                               -- target_f411.mk lands in Phase 2 of the port,
                               currently fails with a clear "no such file"
                               error)

> make TARGET=f103 clean       (cleans only the f103 build tree)


II. Flashing binaries to MCU

Bootloader and application have different base addresses in flash memory:

bootloader base address:	0x8000000
bootloader flash size: 		0x2000

application base address:	0x8002000
application flash size:		0xC800

So as you can see FreeJoy application have 0x2000 offset from the start of the flash memory (0x8000000). If you flashing .bin files the offset must be applied to the start address.
