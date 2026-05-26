# Layer 1 — What FreeJoyX is

## One sentence

FreeJoyX is **firmware** — a program that runs directly on a small STM32
microcontroller and makes it act like a USB game controller. You wire physical
controls (potentiometers, buttons, rotary encoders, hall-effect sensors) to the
chip's pins; the firmware reads them, processes the values, and reports them to a
PC as standard joystick axes and buttons.

No PC driver is needed, because the device speaks a standard USB language called
**HID** (Human Interface Device) — the same class your keyboard and mouse use.

## The whole system in one picture

```
 Physical input        MCU peripheral        Processing            USB HID report      PC
 ───────────────       ───────────────       ──────────────        ───────────────     ──────────────
 potentiometer    ───► ADC               ─┐
 button          ───► GPIO              ─┤
 rotary encoder  ───► timer / GPIO      ─┼──► filter, calibrate ──► packed struct  ──► Windows/Linux
 hall sensor     ───► SPI / I2C         ─┘    deadband, curve,      (axes + buttons)    joystick API
                                              invert, combine
```

The firmware runs this pipeline thousands of times per second. Everything in the
deeper layers is just *how each box works*.

## Three things FreeJoyX is made of

1. **The firmware** (this repo, `anpeaco/FreeJoyX`) — runs on the chip. Split into
   an **application** (the big program that does the joystick work) and a
   **bootloader** (a tiny program that can flash a new application over USB).
2. **The configurator** (`anpeaco/FreeJoyXConfiguratorQt`, and a Rust rewrite in
   `anpeaco/FreeJoyXConfigurator`) — a desktop GUI that reads your settings off the
   device, lets you edit them, and writes them back.
3. **The wire format** — a shared definition of the bytes the firmware and the
   configurator exchange. Both sides must agree on it exactly. See
   [Layer 10](10-wire-format-and-versioning.md).

## What you configure

Through the configurator you decide:

- **Pins** — what each physical pin does (button input, analog axis, SPI bus, LED…).
- **Axes** — calibration, dead zones, response curves, inversion, filtering.
- **Buttons** — which physical input maps to which logical button, and its
  behaviour (normal, toggle, POV hat, radio group, gesture, boolean logic…).
- **Encoders, LEDs, shift registers, sensors** — the rest of the hardware.

All of that lives in one big C structure, `dev_config_t`, which is stored in the
chip's flash so it survives a power-off. See [Layer 9](09-configuration.md).

## Why an STM32 and not, say, an Arduino?

The STM32 chips used here (the F103 "BluePill" and F411 "BlackPill") are cheap,
widely available, and — crucially — have a **native USB peripheral**. That lets the
device be a *real* USB HID device the OS recognises directly, with low latency and
no intermediate firmware translating a serial protocol. The next layer explains
what that chip actually is.

---
Next: [Layer 2 — STM32 fundamentals](02-stm32-fundamentals.md)
