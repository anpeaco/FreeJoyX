# FreeJoyX

**STILL IN INITIAL DEVELOPMENT STAGES**

*Repo is up for feedback and suggestions, I'll update this when it's ready to try.*


<img src="https://github.com/FreeJoy-Team/FreeJoy/blob/master/images/main.png">

FreeJoyX is a fork of [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy) — a widely configurable USB HID game-device firmware. It allows you to build your own HOTAS, pedals, steering wheel, sim-racing button box, etc., or customize a purchased one. FreeJoyX extends upstream FreeJoy with a second hardware-quadrature encoder, boolean-logic virtual buttons, long-press and double-tap gesture button types, and a port to the STM32F411 BlackPill alongside the original BluePill.

## Supported boards

| Target | MCU | Driver layer | USB stack |
|---|---|---|---|
| `f103` (BluePill) | STM32F103C8T6 | StdPeriph | USB-FS-Device |
| `f411` (WeAct BlackPill V3.x) | STM32F411CEU6 | STM32 LL (+ HAL flash driver) | ST USB Device Library |

Both targets share the same `dev_config_t` wire format; the configurator dispatches per-board pin tables based on a `board_id` byte added in firmware v1.7.7 and rejects cross-board configuration writes. Wire format is currently **v1.7.8** (`FIRMWARE_VERSION 0x1780`).

## Getting started

See the upstream [FreeJoy wiki](https://github.com/FreeJoy-Team/FreeJoyWiki) for the original feature set and flashing/configuration walkthrough. The features added by FreeJoyX are documented in the plan files alongside this repo (`F103_FASTENC_PLAN.md`, `F103_LOGIC_PLAN.md`, `F411_PORT_PLAN.md`, `F103_GESTURE_PLAN.md`).

## Features

FreeJoyX supports the following external periphery:

- 8 analog inputs (12 bit output resolution)
- axis-to-buttons function (up to 12 buttons per axis)
- buttons/encoders to axis function
- 128 digital inputs (buttons, toggle switches, hat povs, encoders, **logic-driven virtual buttons**, **long-press**, **double-tap**)
- 8 shift modifiers (bumped from 5 in v1.7.8)
- 4 hat povs
- **2 hardware-quadrature (fast) encoders** — Enc 1 on TIM1 (PA8/PA9), Enc 2 on TIM4 (PB6/PB7), opt-in
- 16 software incremental encoders
- shift registers 74HC165 and CD4021
- digital sensors TLE5010/5011, TLE5012B, AS5048A, AS5600, MLX90393 (SPI interface only)
- external ADCs ADS1115 and MCP3201/02/04/08
- 4 PWM channels for lighting
- 24 LEDs (single or matrix) bindable to button states or controlled by host software
- 50 addressable LEDs WS2812B or PL9823 with effects and SimHub control
- device name and other USB settings

## Axes

FreeJoyX supports up to 8 analog inputs at pins A0–A7 and digital sensors as axis sources. Every axis has its own settings, including:

* Source/destination (X, Y, Z, Rx, Ry, Rz, Slider1, Slider2)
* Output enabling/disabling
* Resolution
* Calibration (manual or auto)
* Smoothing (7 levels of filtering)
* Inversion
* Deadband (dynamic or center)
* Axis offset option (magnet offset)
* Curve shaping
* Functions for combined axes
* Buttons from axes
* Axes from buttons/encoders

## Buttons

Up to 128 digital inputs can be wired as single inputs (tied to VCC or GND), button matrices, shift register inputs, or axis-to-buttons inputs. Each slot can be configured as:

* Regular push button
* Inverted push button
* Toggle switch ON/OFF, ON, or OFF
* POV hat button
* Incremental encoder input
* Radio buttons
* Sequential buttons
* 8 shifts
* **Logic** — boolean function of two physical buttons (`AND`, `OR`, `NOT`, `NOR`, `NAND`, `XOR`, `A AND NOT B`); ON-ON-ON 3-position switches with 2 GPIOs and binary-encoded rotary switches are first-class use cases
* **Long press** — hold-style virtual button that fires after a global threshold (default 500 ms)
* **Double tap** — hold-while-second-tap-held virtual button within a global window (default 300 ms)

Long-press and double-tap can coexist with `NORMAL` on the same physical input; gesture wins, so `NORMAL` is suppressed if the gesture fires within the window. Mixing them with `TOGGLE`/`RADIO`/`SEQUENTIAL`/`POV`/`ENCODER`/`LOGIC` on the same physical input is blocked by the configurator.

## Building

The firmware lives under `armgcc/` and uses a `make`-based build with `arm-none-eabi-gcc`. On Windows the build must be invoked from MSYS2's MinGW64 shell so the GNU assembler can write its temporary files correctly.

```sh
# Build F103 BluePill firmware
make TARGET=f103

# Build F411 BlackPill firmware
make TARGET=f411

# Produce versioned artefacts in <configurator>/firmware/
make TARGET=f103 install-firmware
make TARGET=f411 install-firmware

# Cross-target release: builds both, copies into the configurator's firmware folder
make release RELEASE_VERSION=v1.7.8
```

Output binaries are named `freejoyx-<board>-<app|boot>-<version>.bin` so the configurator's flasher picks the correct image per connected board.

## FreeJoy Configurator utility

FreeJoyX is paired with [FreeJoyConfiguratorQtX](../FreeJoyConfiguratorQtX) — the matching desktop configurator (forked from [FreeJoyConfiguratorQt](https://github.com/FreeJoy-Team/FreeJoyConfiguratorQt)). The two repos must stay in lockstep: `application/Inc/common_types.h` and `application/Inc/common_defines.h` are kept in manual sync with the configurator's copies on every wire-format change.

<img src="https://github.com/FreeJoy-Team/FreeJoyWiki/blob/master/images/main.jpg" width="800"/>
