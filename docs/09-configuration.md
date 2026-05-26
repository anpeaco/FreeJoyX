# Layer 9 — Configuration model

Everything you can configure lives in one struct, `dev_config_t` (in
`common_types.h`). This layer explains its shape, where it's stored, and how it
moves between the device and the configurator.

## `dev_config_t`: the single source of truth

It exists in three places that must agree byte-for-byte:

1. The device's **RAM** while running (the global `dev_config` in `main.c`).
2. The device's **config-storage flash region** (survives power-off).
3. The **configurator**, which reads/edits/writes it.

Its major fields (abridged):

```c
typedef struct {
    uint16_t firmware_version;   // wire-format generation token (see Layer 10)
    uint8_t  board_id;           // which board self-tagged this config
    char     device_name[26];
    uint16_t button_debounce_ms;
    uint8_t  exchange_period_ms; // HID report cadence
    pin_t    pins[USED_PINS_NUM];          // role of every pin

    axis_config_t axis_config[MAX_AXIS_NUM];   // per-axis calibration/curve/...
    button_t      buttons[MAX_BUTTONS_NUM];    // per-logical-button mapping/type
    uint16_t      button_timer1_ms, ..._timer3_ms;
    uint16_t      tap_cutoff_ms, double_tap_window_ms;  // gesture timing

    axis_to_buttons_t   axes_to_buttons[MAX_AXIS_NUM];  // "axis region -> button"
    shift_reg_config_t  shift_registers[4];
    shift_modificator_t shift_config[MAX_SHIFTS_NUM];
    uint16_t            vid, pid;

    led_pwm_config_t led_pwm_config[4];
    led_config_t     leds[MAX_LEDS_NUM];
    encoder_t        encoders[MAX_ENCODERS_NUM];
    fast_encoder_t   fast_encoders[MAX_FAST_ENCODER_NUM];

    uint8_t   rgb_effect, rgb_count, rgb_brightness;
    argb_led_t rgb_leds[NUM_RGB_LEDS];
    phys_breakdown_t saved_breakdown;   // configurator-only metadata
} dev_config_t;
```

### Bit-packing

Many fields are bit-fields (`deadband_size : 7`, `is_dynamic_deadband : 1`, …) to
keep the struct small enough to fit the config flash region and the chunked HID
transfer. This is why adding a field is delicate — it can shift bytes and change the
struct size, which trips the wire-format guard (see [Layer 10](10-wire-format-and-versioning.md)).

### `app_config_t` — derived runtime facts

`AppConfigInit()` scans `dev_config` once at boot and computes counts
(`app_config_t`: how many axes/buttons/POVs/encoders are actually in use). The HID
report builder uses these so it only emits the controls that exist.

## Storage in flash

`config.c` provides `DevConfigGet()` / `DevConfigSet()`. On the device side these
wrap the BSP flash routines:

- **Read** — copy the config region of flash into the RAM `dev_config`.
- **Write** — erase the config flash region, then program the new bytes word-by-word.

Flash write is the dangerous operation. `DevConfigSet()` checks the result of each
erase/program step and aborts on failure (issue #3), because a failed erase followed
by blind programming silently corrupts config (you can only flip 1→0 bits in
un-erased flash). On F4 there's also a data-cache consideration (the ART
accelerator) around flash writes.

## Read / write protocol with the configurator

The struct is larger than one USB HID report, so it's exchanged in **chunks** over
the config report ID:

- **Read config**: configurator requests chunks; firmware streams the current
  `dev_config` back; the configurator reassembles and parses it.
- **Write config**: configurator sends chunks; firmware reassembles, validates, and
  on success calls `DevConfigSet()` then resets so the new config takes effect.

Validation before accepting a write:

- **Wire-format generation** must match → else reply `0xFE` (reflash needed).
- **board_id** must match the connected board → else reply `0xFD` (wrong board;
  offer the cross-board converter). Splitting these two codes (issue #27) lets the
  configurator give an actionable message instead of a generic "version mismatch."

## INI files

The configurator can save/load a `dev_config_t` to a `.ini` file on disk. Loading an
INI saved for a different board triggers the **cross-board converter** (flip
`board_id`, refresh `firmware_version`, handle the one differing pin slot between
BluePill and BlackPill). See the configurator repo.

---
Next: [Layer 10 — Wire format & versioning](10-wire-format-and-versioning.md)
