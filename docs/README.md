# FreeJoyX Documentation

A layered guide to how FreeJoyX works, from "what is this" down to register-level
detail. Each numbered document is a **layer**: the higher numbers assume you've
read the lower ones. Read top to bottom the first time; use it as a reference
after that.

## How to use this wiki

The docs are organised so you can stop at whatever depth you need:

- **Layers 1–2** — the mental model. No prior embedded experience required.
- **Layers 3–4** — how the firmware is structured and how it runs on the chip.
- **Layers 5–8** — the four subsystems that do the actual work (axes, buttons,
  encoders, USB).
- **Layers 9–13** — configuration, the wire format, flashing, the two boards,
  and how the repos fit together.
- **Deep dives** — end-to-end traces through real code, plus a glossary.

Every claim here is tied to a real file in the tree (e.g.
`application/Src/analog.c`). When the prose and the code disagree, the code wins —
treat a mismatch as a docs bug and fix it.

## Table of contents

### The mental model
1. [What FreeJoyX is](01-what-is-freejoyx.md)
2. [STM32 fundamentals](02-stm32-fundamentals.md)

### How the firmware is built and runs
3. [Memory layout & boot sequence](03-memory-and-boot.md)
4. [The runtime model (setup + superloop)](04-runtime-model.md)

### The subsystems
5. [Axes — the analog pipeline](05-axes.md)
6. [Buttons — sources, debounce, logical types](06-buttons.md)
7. [Encoders](07-encoders.md)
8. [USB & HID](08-usb-hid.md)

### Configuration & lifecycle
9. [Configuration model](09-configuration.md)
10. [Wire format & versioning](10-wire-format-and-versioning.md)
11. [Flashing & DFU](11-flashing-and-dfu.md)
12. [The two boards: F103 vs F411](12-f103-vs-f411.md)
13. [Repos, build & CI](13-repos-and-build.md)

### Cross-cutting
- [State machines](state-machines.md) — a pattern shared by buttons, encoders, USB & DFU

### Deep dives
- [Trace: a button press, end to end](deep-dives/trace-button-press.md)
- [Trace: a config write, end to end](deep-dives/trace-config-write.md)
- [Glossary](glossary.md)

## A note on scope

FreeJoyX is a fork of [FreeJoy-Team/FreeJoy](https://github.com/FreeJoy-Team/FreeJoy).
The fork's headline additions are the **STM32F411 (BlackPill) port**, a reworked
**gesture/logic button** model, a guided **flasher**, and **cross-board** config
handling. Where a concept is inherited from upstream, the docs say so; where it's
fork-specific, they say that too.
