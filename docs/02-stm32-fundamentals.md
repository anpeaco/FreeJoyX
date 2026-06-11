# Layer 2 — STM32 fundamentals

You don't need to be an embedded engineer to work on FreeJoyX, but you need a
handful of concepts. This layer gives you exactly those.

## What a microcontroller is

A **microcontroller (MCU)** is a whole tiny computer on a single chip:

- a **CPU core** (an ARM Cortex-M),
- **flash** memory — non-volatile, holds your program (and survives power-off),
- **RAM** — volatile, holds variables while the program runs (lost on power-off),
- **peripherals** — dedicated hardware blocks that do specific jobs.

Unlike your laptop, there's no hard drive and no operating system. The firmware
*is* the only software, and it owns the chip completely.

## The peripherals that matter for FreeJoy

A peripheral is hardware the CPU configures and then mostly lets run on its own.
FreeJoyX uses:

| Peripheral | What it does | Used for |
|---|---|---|
| **GPIO** | Read/drive a pin as digital high/low | Buttons, LEDs, bit-banged buses |
| **ADC** | Convert a pin voltage to a number (0–4095 at 12-bit) | Potentiometers, analog pedals |
| **Timer** | Count time or pulses; generate PWM | Encoders, LED PWM, periodic ticks |
| **SPI** | Fast serial bus to external chips | Hall sensors (TLE5011, AS5048A…), MCP320x ADCs |
| **I2C** | Two-wire serial bus to external chips | ADS1115 ADC, AS5600 sensor, OLED |
| **USB** | Talk to the host PC | The whole point — HID reports + config + DFU |

You'll see these names throughout the code. For example, in
`application/Src/analog.c`, `AxesInit()` configures the **ADC** and a **DMA**
channel to stream conversions into a buffer; the SPI sensor drivers
(`tle5011.c`, `as5048a.c`, …) talk over **SPI**.

## "Registers": how you talk to peripherals

Peripherals are controlled by reading and writing special memory addresses called
**registers**. Writing a bit pattern to a register configures or commands the
hardware. You'll see two styles in this codebase:

- **Vendor library calls** that wrap the registers in functions, e.g.
  `ADC_Init(ADC1, &cfg)` (the F103 path uses ST's older *StdPeriph* library).
- **Direct register writes**, e.g. `ADC1->SQR1 = sqr1;` (the F411 path pokes
  registers directly, with CMSIS field macros like `DMA_SxCR_MSIZE_0`).

Both do the same kind of thing; the F411 port just uses the lower-level style. See
[Layer 12](12-f103-vs-f411.md) for why the two paths exist.

## DMA — moving data without the CPU

**DMA** (Direct Memory Access) is a peripheral that copies data between other
peripherals and memory *without* the CPU doing the work. FreeJoy uses it so the
ADC can continuously deposit conversion results into an array (`adc_data[]`) in
the background. (The infamous fixed bug #24 was a DMA transfer-size mistake — word
vs halfword — that corrupted multi-axis ADC reads.)

## Interrupts — reacting to events

An **interrupt** is a hardware event that briefly pauses the main program, runs a
short handler function (an ISR — Interrupt Service Routine), then resumes. This is
how a USB packet gets serviced "in the background." On F103 the USB interrupt
handler lives in the USB library; timer interrupts drive periodic sensor reads.
Keep ISRs short — they steal time from everything else.

## Clocks

Every peripheral runs off a **clock**. Before you can use a peripheral you must
enable its clock (you'll see `RCC->...EN` bits being set). The chip also has a
master clock tree configured at boot — the F411 BSP locks in a 96 MHz HSE-PLL
recipe (`Board_ClockInit_F411`), the F103 runs at 72 MHz. Get the clock wrong and
USB timing (which needs exactly 48 MHz) breaks.

## The key mental shift

On a PC you think in terms of processes and files. On an MCU you think in terms of
**peripherals you configure once** and **a loop that runs forever**. That loop is
the subject of [Layer 4](04-runtime-model.md) — but first, [Layer 3](03-memory-and-boot.md)
explains how the program is laid out in flash and how it starts.

---
Next: [Layer 3 — Memory layout & boot sequence](03-memory-and-boot.md)
