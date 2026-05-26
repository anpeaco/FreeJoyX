# Layer 13 — Repos, build & CI

How the pieces fit together across repositories, how the firmware is built, and what
CI enforces.

## The three repositories

| Repo | What it is | Language |
|---|---|---|
| `anpeaco/FreeJoyX` | The firmware (this repo) | C (+ ARM asm) |
| `anpeaco/FreeJoyXConfiguratorQt` | The desktop configurator GUI | C++ / Qt 5 |
| `anpeaco/FreeJoyXConfigurator` | A newer Rust + Slint rewrite of the GUI | Rust |

The firmware and a configurator talk over USB HID. The two configurators are
alternative front-ends for the same device; the Rust one is an in-progress port
mirroring the Qt one's behaviour.

## Firmware repo layout

```
application/        the big program (joystick logic + USB + config)
  Src/ Inc/         analog.c, buttons.c, encoders.c, config.c, main.c, usb_*.c, sensor drivers
bootloader/         the DFU flasher (F103)
  f411/             the F411 bootloader
board/              the BSP seam: f103_bluepill/ and f411_blackpill/
armgcc/             makefiles (makefile / makefile.boot) for the GCC build
common_*.h          the synced wire-format headers (in application/Inc/)
docs/               this documentation
```

## Building

The firmware builds with the ARM GCC toolchain (`arm-none-eabi-gcc`) via the
makefiles in `armgcc/`. Typical invocations:

```
make TARGET=f103     # build the F103 application + bootloader
make TARGET=f411     # build the F411 application + bootloader
```

Output is `.bin` images (application and bootloader, per board). There are also
CLion/MDK-ARM project files for IDE builds.

> Note: a real build needs the ARM toolchain installed. A plain dev container
> without `arm-none-eabi-gcc` can edit and reason about the code but can't produce
> firmware binaries — rely on CI for the actual compile.

## CI (GitHub Actions)

On every PR the firmware repo runs (you can see these on PR checks):

- **Build f103** / **Build f411** — full ARM builds of both targets. This is the
  real compile gate.
- **Compare common_*.h with configurator** — diffs the synced wire-format headers
  against the configurator repo and fails on drift (outside `SYNC_SKIP` markers).
  See [Layer 10](10-wire-format-and-versioning.md).
- **CodeQL (Analyze c-cpp / actions)** — static analysis.

On a `v*` tag, a release workflow builds and publishes the
`freejoyx-{f103,f411}-{app,boot}-vX.Y.Z.bin` artifacts.

## Release lockstep

Because the wire-format headers are mirrored, a `FREEJOYX_VERSION` bump must land in
both the firmware and the configurator together, or the header-sync CI goes red.
Version-bump PRs come in pairs (one per repo) referencing each other.

---
Back to the [index](README.md). For end-to-end walkthroughs, see the
[deep dives](deep-dives/trace-button-press.md).
