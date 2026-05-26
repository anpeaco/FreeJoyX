# Glossary

Quick definitions of terms used across the docs. Repo-specific terms are marked.

- **ADC** — Analog-to-Digital Converter. MCU peripheral that turns a pin voltage
  into a number (0–4095 at 12-bit). Used for potentiometers/pedals.
- **app_config_t** *(repo)* — derived runtime counts (axis/button/POV totals)
  computed from `dev_config_t` at boot.
- **axis_config_t** *(repo)* — per-axis settings (calibration, curve, deadband,
  inversion, source). In `common_types.h`.
- **Bare metal** — running with no operating system; the firmware owns the chip.
- **BSP** — Board Support Package. The per-chip code behind `Board_*` functions.
- **Bootloader** — the small program that flashes the application over USB.
- **CMSIS** — ARM's standard headers defining register names/bit macros for Cortex-M.
- **Debounce** — filtering the contact bounce of a mechanical switch.
- **Deadband / dead zone** — a range around center treated as "no movement."
- **dev_config_t** *(repo)* — the single struct holding all device configuration.
- **DFU** — Device Firmware Update; flashing over USB.
- **DMA** — Direct Memory Access; moves data without CPU involvement.
- **Dynamic deadband** *(repo)* — variance-based hold that freezes a jittering axis.
- **Endpoint** — a USB data pipe. EP0 is control; others carry reports.
- **Enumeration** — the USB handshake where the host learns what the device is.
- **FIRMWARE_VERSION** *(repo)* — wire-format generation token (`& 0xFFF0` mask).
- **FREEJOYX_VERSION** *(repo)* — human release version, reported live to the GUI.
- **FIR filter** — finite-impulse-response low-pass filter used to smooth axes.
- **Flash** — non-volatile program/config memory; survives power-off.
- **GPIO** — General-Purpose I/O pin.
- **HAL / LL** — ST's Cube libraries (High Abstraction / Low Layer); used on F411.
- **HID** — Human Interface Device; the USB class that needs no custom driver.
- **I2C** — two-wire serial bus to external chips (ADS1115, AS5600).
- **Interrupt / ISR** — hardware event that pauses the program to run a handler.
- **joy_report_t** *(repo)* — the HID report the OS sees (axes, POVs, buttons).
- **LOGIC button** *(repo)* — a button whose state is a boolean expression of two
  inputs.
- **Magic word** *(repo)* — value (`0x424C`) in a backup register that tells the
  bootloader to stay in DFU.
- **map2 / map3** *(repo)* — range-remap functions in `analog.c` (map3 adds
  calibration center + deadband).
- **params_report_t** *(repo)* — live-diagnostic HID report the configurator polls.
- **Peripheral** — a hardware block (ADC, USB, timer…) the CPU configures.
- **POV** — Point-Of-View hat switch (the 4/8-way thumb hat).
- **Quadrature** — two phase-shifted signals encoding rotation direction (encoders).
- **Register** — a special memory address that controls a peripheral.
- **Report descriptor** — HID grammar telling the host the shape of each report.
- **Report ID** — first byte of a HID report, selecting which channel it is.
- **Shift register** *(hardware)* — external chip (74HC165/CD4021) that expands
  inputs, read serially.
- **shift_modificator** *(repo)* — a "shift layer" letting one input mean different
  things per layer.
- **StdPeriph** — ST's older peripheral library; used on F103.
- **Superloop** — the `while(1)` that runs forever after setup.
- **SWD** — Serial Wire Debug; 2-pin interface for an ST-Link to flash/debug.
- **SysTick** — Cortex-M core timer; the 1 ms time base behind `GetMillis()`.
- **TAP / DOUBLE_TAP** *(repo)* — gesture button types based on press timing.
- **VTOR** — Vector Table Offset Register; tells the CPU where interrupt handlers are.
- **Wire format** — the agreed byte layout of structs exchanged firmware↔configurator.
