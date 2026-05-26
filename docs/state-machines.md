# Cross-cutting: State machines

This is a *cross-cutting* doc, not a numbered layer. It collects a pattern that
shows up in several subsystems so you can recognise it once and read the rest of
the wiki faster. It assumes you've read [Layer 4 — the runtime model](04-runtime-model.md);
the references point back into [buttons](06-buttons.md), [encoders](07-encoders.md),
[USB](08-usb-hid.md), and [flashing](11-flashing-and-dfu.md).

## What a state machine is, and why firmware is full of them

A state machine remembers what state it's in and, on each tick, computes its next
state from `(current state + new input)`. Nothing more.

Firmware leans on the pattern because the physical world is full of inputs whose
*meaning depends on history*. "This pin reads high" tells you nothing on its own.
"This pin reads high but read low last tick" is a **press event**. To turn a stream
of instantaneous readings into events and behaviors, you have to remember the past —
and a small struct of state plus a pure `next = f(state, input)` step is the cheapest
honest way to do that.

It also fits the embedded sweet spot: a fixed-rate tick (the superloop / timer from
[Layer 4](04-runtime-model.md)), one fixed-size struct of state per object, no dynamic
allocation, and bounded work per tick.

## 1. The quadrature encoder decoder — a lookup-table machine

Code: `application/Src/encoders.c`. This is the textbook example, expressed as a
*table* instead of a tree of `if`s.

A rotary encoder emits two square waves, phases A and B, 90° out of phase. Direction
is encoded purely in the *order* edges arrive, so a single reading is meaningless —
you need the transition from the previous reading to the current one.

The state is the previous A/B pair. Each step packs `(prev_AB << 2) | curr_AB` into a
4-bit index (0–15) and looks the result up:

```c
const int8_t enc_array_4 [16] = {
    0,  1, -1,  0,
   -1,  0,  0,  1,
    1,  0,  0, -1,
    0, -1,  1,  0
};
```

- `0` — no movement, or an illegal/bounced transition. (Note the zeros on the
  diagonal: a reading that claims both phases flipped at once is impossible in one
  step, so it's ignored — free debounce.)
- `+1` / `-1` — one step of forward / backward motion.

Three tables exist — `enc_array_1 / _2 / _4` (encoders.c:29-51) — for the 1×/2×/4×
resolution modes (`ENCODER_CONF_1x/_2x/_4x`). `_1x` counts one transition per cycle,
`_4x` counts all four. The per-encoder state lives in `encoder_state_t`
(`common_types.h`): `state` is the remembered A/B pair, `dir`/`last_dir` the decoded
direction, `cnt` the accumulated count downstream code reads.

That's the whole decoder: history-as-index, no branches.

## 2. The logical button — a family of machines over one input

Types & state: `common_types.h`; processing in `application/Src/buttons.c`. See
[Layer 6](06-buttons.md) for the full picture.

A physical pin is first debounced into a clean boolean. Then a *logical* layer turns
that boolean stream into behavior — and that layer is a state machine whose ruleset
is selected by the button's `type`. The memory is `logical_buttons_state_t`, and the
bitfields *are* the state:

- `curr_physical_state` / `prev_physical_state` — this tick vs last tick. Their
  difference is the **edge**: rising = press event, falling = release event.
- `current_state` — the logical output actually reported over USB.
- `on_state` / `off_state`, `delay_act` — latch and timing bookkeeping.

Different `type` values are *different machines over the same input*:

- **BUTTON_NORMAL** — output mirrors the debounced input. Effectively stateless.
- **BUTTON_TOGGLE / TOGGLE_SWITCH** — output *flips* on each rising edge, so the
  output depends on accumulated history rather than the current reading.
- **TAP** — fires only if press *and* release both land inside `tap_cutoff_ms`;
  holding past the cutoff aborts the transition without firing, letting a sister
  NORMAL slot take the hold.
- **DOUBLE_TAP** — an explicit three-state machine, documented right on the struct:
  `tap_count` runs `0 = idle → 1 = first tap seen (waiting for the second within the
  window) → 2 = captured (mirror physical until release)`. `first_tap_ms` timestamps
  the window so an un-completed double-tap times out back to idle.

`release_floor` is a minimum-hold deadline: a gesture-derived pulse stays high until
`millis > release_floor` even after its condition drops, so a fast host still sees the
event (see [#22](https://github.com/anpeaco/FreeJoyX/issues/22)).

## 3. Two more, handled mostly for you

- **USB enumeration** ([Layer 8](08-usb-hid.md)) — the device walks the standard USB
  state machine (`Default → Addressed → Configured`) as the host assigns an address
  and selects a configuration. The ST USB stack owns these transitions; FreeJoyX just
  supplies the descriptors and report callbacks.
- **The DFU chunked-flash protocol** ([Layer 11](11-flashing-and-dfu.md)) — a
  request/response machine: `idle → receiving chunks → verify CRC → done / clean
  disconnect`. The clean-disconnect terminal state is the fix from
  [#36](https://github.com/anpeaco/FreeJoyX/issues/36).

## The shared shape

Look past the specifics and all four are the same thing:

1. A fixed-rate tick drives one step.
2. Each object carries a small, fixed-size state struct.
3. The step is a pure function of `(state, input)` — table lookup, edge test, or
   timer comparison.

Once you see that shape, the encoder tables and the button bitfields stop looking
like ad-hoc tricks and read as the same idea in different clothes.

---
Back to the [index](README.md). Related: [Buttons](06-buttons.md) ·
[Encoders](07-encoders.md) · [USB & HID](08-usb-hid.md) · [Flashing & DFU](11-flashing-and-dfu.md)
