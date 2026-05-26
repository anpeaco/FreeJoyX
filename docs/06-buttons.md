# Layer 6 — Buttons: sources, debounce, logical types

Buttons look simple but carry a lot of behaviour. There are two halves: **physical
buttons** (the electrical inputs) and **logical buttons** (what the host sees,
after type/gesture/logic processing). Code: `application/Src/buttons.c`; types in
`common_types.h`.

## Physical sources

A "physical button" can come from three wiring styles, chosen by the `pins[]` map
(`pin_t` enum in `common_types.h`):

1. **Matrix** — `BUTTON_ROW` × `BUTTON_COLUMN` pins. The firmware drives one row at
   a time and reads the columns, so N+M pins read N×M buttons. Cheap way to get many
   buttons.
2. **Shift registers** — `SHIFT_REG_DATA / _LATCH / _CLK` pins clock in external
   74HC165 / CD4021 chips, each adding 8 inputs, chainable. See `shift_registers.c`
   and `shift_reg_config_t`.
3. **Direct** — `BUTTON_GND` / `BUTTON_VCC` pins: one pin, one button, pulled to
   ground or VCC.

All three are flattened into a single numbered list of **physical button indices**.
That index is what an axis trim button or a logical button refers to.

## Debounce

A mechanical switch doesn't transition cleanly — it "bounces," making and breaking
contact several times over a few milliseconds. `ButtonsDebounceProcess()` filters
this: a state change only counts once it's been stable for `button_debounce_ms`.
Each physical button tracks `pin_state / prev_pin_state / current_state / changed`
and a `time_last` timestamp (`physical_buttons_state_t`).

## Logical buttons and their types

`ButtonsReadLogical()` turns physical states into the **logical buttons** the host
sees. Each logical button (`button_t`) has a `type` (`button_type_t`). The types,
straight from the enum:

- **NORMAL** — pressed = pressed.
- **TOGGLE / TOGGLE_SWITCH / _ON / _OFF** — press flips a latched state; switch
  variants track a physical on/off switch.
- **POV1..POV4 _UP/_DOWN/_LEFT/_RIGHT/_CENTER** — hat-switch directions; the report
  layer turns a group into a single POV angle.
- **ENCODER_INPUT_A / _B** — feed an encoder (see [Layer 7](07-encoders.md)).
- **RADIO_BUTTON1..4** — only one in the group is active at a time
  (`RadioButtons_Init`).
- **SEQUENTIAL_TOGGLE / SEQUENTIAL_BUTTON** — step through a sequence on each press
  (`SequentialButtons_Init`).
- **LOGIC** — the button's state is a boolean expression of two other inputs (see
  below).
- **TAP / DOUBLE_TAP** — gesture types (see below).

### Modifiers on every logical button

`button_t` also carries: `shift_modificator` (0 = none, 1..8 = a shift layer — like a
keyboard's shift, lets one physical button mean different things per layer),
`is_inverted`, `is_disabled`, and two timer selectors (`delay_timer`, `press_timer`)
referencing `button_timer1/2/3_ms`.

### LOGIC buttons

When `type == LOGIC`, the slot computes a boolean expression:
`physical_num` is **Source A**, `src_b` is **Source B**, and `op` selects the
operator (`logic_op_t`): AND, OR, NOT, NOR, NAND, XOR, A_AND_NOT_B, XNOR. NOT is
unary (ignores Source B). This lets you build things like "this output is active
only when button 3 AND button 7 are pressed" without host-side macros.

### Gesture buttons (TAP / DOUBLE_TAP)

These are fork-specific (the v0.1.0 rework). They depend on timing:

- **TAP** fires a brief pulse when a physical input is pressed **and released within
  `tap_cutoff_ms`**. Holding past the cutoff aborts without firing. (The enum value
  is the old `LONG_PRESS` slot, reinterpreted — kept numerically stable so old
  configs still decode.)
- **DOUBLE_TAP** fires when two taps happen within `double_tap_window_ms`.

The runtime state (`logical_buttons_state_t`) tracks a small state machine
(`button_action_t`: IDLE → DELAY → PRESS → BLOCK, plus `TAP_PENDING`), `tap_count`,
`first_tap_ms`, and a `release_floor` (a minimum-hold deadline so a fired gesture
stays high long enough for the host to register it). Coexistence rule: a single
physical input may host only `{NORMAL, TAP, DOUBLE_TAP}` together; the configurator
enforces this. When a NORMAL slot shares a physical with a gesture slot, NORMAL
defers its output by the gesture window, and the gesture "wins" if it fires.

## What reaches the host

After resolution, each logical button is one bit. The report layer packs them into
`joy_report_t.button_data[MAX_BUTTONS_NUM/8]` (one bit per button). POV groups are
collapsed into `pov_data[]` angles. See [Layer 8](08-usb-hid.md).

The live-diagnostic stream (`params_report_t`) additionally exposes
`phy_button_data` (raw physical states) and `log_button_data` (resolved logical
states) — that's what the configurator's live preview and Sequential-Assign feature
read.

The TAP / DOUBLE_TAP / TOGGLE logic is best understood as a family of small state
machines over one debounced input — see [State machines](state-machines.md).

---
Next: [Layer 7 — Encoders](07-encoders.md)
