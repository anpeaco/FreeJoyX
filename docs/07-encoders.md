# Layer 7 — Encoders

A rotary encoder reports *rotation*, not absolute position, via two phase-shifted
signals (A and B) — "quadrature." The direction of rotation is encoded in which
signal leads. Code: `application/Src/encoders.c`; types in `common_types.h`.

## Two kinds: slow and fast

FreeJoy supports two encoder implementations:

- **Slow (software) encoders** — A and B are ordinary button inputs
  (`ENCODER_INPUT_A` / `ENCODER_INPUT_B` button types). The firmware decodes the
  quadrature in software by watching the two states change. Limited by the polling
  rate, fine for hand-turned knobs.
- **Fast (hardware) encoders** — wired to pins that the chip's **timer** peripheral
  can decode in hardware quadrature mode (`FAST_ENCODER` pin type). The timer counts
  pulses directly, so it keeps up with high-speed rotation the CPU couldn't poll
  fast enough. On F103 the pin pairs are silicon-locked to specific timers
  (TIM1→PA8/PA9, TIM4→PB6/PB7), so they aren't in the per-encoder config struct —
  they live in a static `fast_encoder_hw[]` table.

## Configuration

- `encoders[MAX_ENCODERS_NUM]` — per slow-encoder mode.
- `fast_encoders[MAX_FAST_ENCODER_NUM]` (`fast_encoder_t`) — `enabled` + `mode`
  (`ENCODER_CONF_1x / _2x / _4x`). The multiplier sets how many counts per detent:
  1×, 2×, or 4× per quadrature cycle.

## Runtime state

`encoder_state_t` tracks `cnt` (the accumulated count), `dir` / `last_dir`
(direction), `state` (the quadrature state machine), and `time_last`. The `cnt` is
what downstream code reads.

## How an encoder becomes output

Two ways:

1. **As buttons** — each rotation step pulses a logical button (CW button, CCW
   button). Common for menu navigation.
2. **As an axis** — an axis with `source_main == SOURCE_ENCODER` reads the encoder's
   `cnt` (clamped to `calib_min..calib_max`) and runs it through the normal axis
   pipeline ([Layer 5](05-axes.md)). This turns a knob into a continuous axis with
   software end-stops.

`encoder_press_time_ms` controls how long an encoder-driven button pulse is held so
the host reliably sees each step.

---
Next: [Layer 8 — USB & HID](08-usb-hid.md)
