# FreeJoyX

[![Firmware build](https://github.com/anpeaco/FreeJoyX/actions/workflows/firmware.yml/badge.svg)](https://github.com/anpeaco/FreeJoyX/actions/workflows/firmware.yml)
[![Wire-format header sync](https://github.com/anpeaco/FreeJoyX/actions/workflows/header-sync.yml/badge.svg)](https://github.com/anpeaco/FreeJoyX/actions/workflows/header-sync.yml)

> **FreeJoyX project:** [Firmware](https://github.com/anpeaco/FreeJoyX) · [Configurator](https://github.com/anpeaco/FreeJoyXConfiguratorQt)

**STILL IN INITIAL DEVELOPMENT STAGES**

*Repo is up for feedback and suggestions, I'll update this when it's ready to try.*


<img src="https://github.com/FreeJoy-Team/FreeJoy/blob/master/images/main.png">

FreeJoyX is a fork of [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy) — a widely configurable USB HID game-device firmware. It allows you to build your own HOTAS, pedals, steering wheel, sim-racing button box, etc., or customize a purchased one. FreeJoyX extends upstream FreeJoy with a second hardware-quadrature encoder, boolean-logic virtual buttons, tap and double-tap gesture button types, and a port to the STM32F411 BlackPill alongside the original BluePill.

## Supported boards

| Target | MCU | Driver layer | USB stack |
|---|---|---|---|
| `f103` (BluePill) | STM32F103C8T6 | StdPeriph | USB-FS-Device |
| `f411` (WeAct BlackPill V3.x) | STM32F411CEU6 | STM32 LL (+ HAL flash driver) | ST USB Device Library |

Both targets share the same `dev_config_t` wire format; the configurator dispatches per-board pin tables based on a `board_id` byte added in firmware v1.7.7 and rejects cross-board configuration writes (with the configurator's cross-board converter to bridge the gap). Wire format generation is currently `FIRMWARE_VERSION 0x0020` (released as **v0.1.x**); the prior `0x0010` generation and the upstream `0x17XX` lineage are still readable and forward-migratable by the configurator.

## Getting started

See the upstream [FreeJoy wiki](https://github.com/FreeJoy-Team/FreeJoyWiki) for the original feature set and flashing/configuration walkthrough. The features added by FreeJoyX are documented in the plan files alongside this repo (`F103_FASTENC_PLAN.md`, `F103_LOGIC_PLAN.md`, `F411_PORT_PLAN.md`, `F103_GESTURE_PLAN.md`).

## Features

FreeJoyX supports the following external periphery:

- 8 analog inputs (12 bit output resolution)
- axis-to-buttons function (up to 12 buttons per axis)
- buttons/encoders to axis function
- **axis auto-detect** (v0.1.3+) — reports a raw value per `AXIS_ANALOG` pin so the configurator can identify a rotated pot before it's mapped to an axis (see [Axes](#axes))
- 128 digital inputs (buttons, toggle switches, hat povs, encoders, **logic-driven virtual buttons**, **tap**, **double-tap**)
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

Since **v0.1.3** the params report carries `detect_axis_raw[]` — a raw value
per `AXIS_ANALOG` pin (PA0–PA7), sampled whether or not an axis sources the
pin — so the configurator's rotate-to-detect can identify an analog pot even
before it's mapped to a logical axis (the analog equivalent of the physical
button bitmap). Params-report-only change: `dev_config_t` is unchanged, so no
factory reset. External SPI/I2C sensors are not covered (their addressing is
axis-bound). See `AXIS_DETECT_PLAN.md`.

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
* **Logic** — boolean function of two physical buttons (`AND`, `OR`, `NAND`, `NOR`, `XOR`, `XNOR`); ON-ON-ON 3-position switches with 2 GPIOs and binary-encoded rotary switches are first-class use cases. (`NOT` and `A AND NOT B` exist in the wire-format enum for back-compat with shipped configs but are no longer offered in the configurator picker — `NOT A` duplicates `NORMAL` + invert, and the inhibit-gate pattern can be built from `AND` with a NORMAL+invert slot for B.)
* **Tap** — release-within-cutoff virtual button: fires briefly when the physical is pressed and released within the global cutoff window (default 200 ms). Holding past the cutoff aborts without firing, letting any sister `NORMAL` slot take the hold.
* **Double tap** — hold-while-second-tap-held virtual button within a global window (default 200 ms). The output mirrors the physical while the second tap is held.

Tap and double-tap can coexist with `NORMAL` on the same physical input; gesture wins, so `NORMAL` is suppressed if the gesture fires within the window. Mixing them with `TOGGLE`/`RADIO`/`SEQUENTIAL`/`POV`/`ENCODER`/`LOGIC` on the same physical input is blocked by the configurator's per-physical coexistence filter.

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
make release RELEASE_VERSION=v0.1.1
```

Output binaries are named `freejoyx-<board>-<app|boot>-<version>.bin` so the configurator's flasher picks the correct image per connected board.

## Config write rejection codes

When the configurator writes a config to the device, the firmware validates the incoming `dev_config_t` after the last packet arrives. On rejection it returns a single byte in `REPORT_ID_CONFIG_OUT`:

| Code | Reason | User action |
|------|--------|-------------|
| `0xFE` | Wire-format generation mismatch (`firmware_version & 0xFFF0` doesn't match the running firmware) | Reflash to a firmware version on the same wire-format generation as the config, or load a config saved at the running firmware's generation |
| `0xFD` | `board_id` mismatch (config was saved for a different board) | Load a config saved for the connected board, or use the configurator's cross-board converter on the load path |

Older firmware that predates the split (anpeaco/FreeJoyX#27) sends `0xFE` for both cases. Newer configurators handle both byte codes; older configurators silently ignore `0xFD`. See `application/Src/usb_app.c` "Last packet received. Check version + board_id" for the firmware-side logic.

## Continuous integration

Three GitHub Actions workflows run on every push:

- **`firmware.yml`** — matrix-builds both `f103` and `f411` (app + boot) on Ubuntu with `gcc-arm-none-eabi`, uploads versioned `.bin` artifacts.
- **`header-sync.yml`** — clones [`anpeaco/FreeJoyXConfiguratorQt`](https://github.com/anpeaco/FreeJoyXConfiguratorQt) as a sibling and diffs `common_types.h` + `common_defines.h` after stripping comments and whitespace. Catches wire-format drift between firmware and configurator. Firmware-only sections are wrapped in `/* SYNC_SKIP_BEGIN ... SYNC_SKIP_END */` markers so the check ignores them. A mirror workflow runs in the configurator repo on its own pushes; both sides catch drift independently.
- **`release.yml`** — on `v*` tag push, runs `make release RELEASE_VERSION=<tag>` and publishes the four binaries (F103 app + boot, F411 app + boot) to a GitHub Release with auto-generated notes.

Tagging the same `vX.Y.Z` here and on the [configurator repo](https://github.com/anpeaco/FreeJoyXConfiguratorQt) in lockstep produces a matched release pair: four firmware `.bin`s here, plus the configurator's `FreeJoyXConfiguratorQt-linux-<tag>.tar.gz` and `FreeJoyXConfiguratorQt-windows-<tag>.zip` (self-contained `windeployqt` bundle) on that side. The configurator's release workflow additionally supports `workflow_dispatch` for retro-adding a missing platform asset to an existing release.

To cut a release locally:

```bash
git tag v0.1.2
git push origin v0.1.2
# Release workflow builds + publishes the binaries automatically.
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build, commit, and PR guidance.
See [STYLE.md](STYLE.md) for the inherited code style and the wire-format
lockstep rule.

## FreeJoy Configurator utility

FreeJoyX is paired with [FreeJoyXConfiguratorQt](../FreeJoyXConfiguratorQt) — the matching desktop configurator (forked from [FreeJoyConfiguratorQt](https://github.com/FreeJoy-Team/FreeJoyConfiguratorQt)). The two repos must stay in lockstep: `application/Inc/common_types.h` and `application/Inc/common_defines.h` are kept in manual sync with the configurator's copies on every wire-format change.

<img src="https://github.com/FreeJoy-Team/FreeJoyWiki/blob/master/images/main.jpg" width="800"/>
