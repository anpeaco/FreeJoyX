# Deep dive — Trace: a button press, end to end

Follow one physical button press from the pin to the bit the PC reads. This ties
together Layers 4, 6, and 8. File references are to `application/`.

## The setup (once, at boot)

`IO_Init(&dev_config)` configures the pin per `dev_config.pins[]`. Say pin 5 is
`BUTTON_GND` — configured as input with a pull-up, so it reads high when open and
low when the button shorts it to ground. The flattening logic assigns this pin a
**physical button index**, say physical #5.

`dev_config.buttons[0]` is a logical button with `physical_num = 5`, `type =
BUTTON_NORMAL`. So "logical button 0 follows physical 5."

## Per superloop iteration

### 1. Sample + debounce — `ButtonsDebounceProcess()`

The pin reads low (pressed). The function records it in
`physical_buttons_state_t.pin_state` for physical #5, and compares against
`prev_pin_state`. The new state is only promoted to `current_state` once it's been
stable for `button_debounce_ms` (using `GetMillis()` vs `time_last`). This rejects
contact bounce. After this call, physical #5's `current_state == 1`.

### 2. Resolve logical buttons — `ButtonsReadLogical()`

For logical button 0 (`type == NORMAL`), the resolver copies physical #5's state to
the logical state (`logical_buttons_state_t.current_state = 1`), applying the
`is_inverted` / `is_disabled` / `shift_modificator` modifiers.

If the type were richer the resolver would do more here:
- **TOGGLE**: flip a latched bit on the rising edge.
- **LOGIC**: evaluate `op(SourceA, SourceB)`.
- **TAP/DOUBLE_TAP**: run the gesture state machine against `GetMillis()` and
  `tap_cutoff_ms` / `double_tap_window_ms`, possibly deferring the output
  (`TAP_PENDING`) or holding it until `release_floor`.

For our NORMAL button, logical 0 is now 1.

## On the timer tick: build + send the report

Separately from the loop, the periodic tick assembles the HID report. The report
builder packs each logical button into one bit of
`joy_report_t.button_data[MAX_BUTTONS_NUM/8]`:

```
button_data[0] |= (logical_state[0] << 0);   // logical button 0 -> bit 0 of byte 0
```

Axes (`out_axis_data[]`) and POV angles are filled in alongside. The completed
`joy_report_t` is written to the USB IN endpoint and sent to the host every
`exchange_period_ms`.

## On the PC

The host already learned the report's shape during enumeration from the HID report
descriptor ([Layer 8](../08-usb-hid.md)). It parses byte 0, bit 0 as "button 1
pressed," and the OS joystick API reports button 1 down. A game polling that API
sees the press.

## End-to-end latency budget

- Debounce: up to `button_debounce_ms` (configurable, often a few ms).
- Loop + tick cadence: a few ms.
- USB poll interval: ~1 ms for a full-speed HID.

So a press surfaces to the game within a handful of milliseconds — dominated by the
debounce window you chose.

## Parallel diagnostic path

The same logical and physical states are also copied into `params_report_t`
(`log_button_data` and `phy_button_data`) and streamed to the configurator, which
is how its live preview lights up and how Sequential-Assign detects which physical
you pressed — without affecting the joystick report at all.

---
See also: [Trace: a config write](trace-config-write.md)
