# Layer 8 — USB & HID

This is how the device talks to the PC. The goal: appear as a standard joystick the
OS recognises with no custom driver, while *also* carrying private channels for
configuration and firmware updates.

## USB in 90 seconds

- A USB device is the **peripheral**; the PC is the **host**. The host initiates
  everything.
- When plugged in, the device goes through **enumeration**: the host asks "who are
  you?" and the device replies with **descriptors** — fixed data structures
  describing itself (vendor/product IDs, endpoints, and what kind of device it is).
- The device exposes **endpoints** — logical pipes for data. Endpoint 0 is the
  control pipe (used during enumeration); other endpoints carry the actual reports.

## HID — the no-driver trick

**HID** (Human Interface Device) is a standard USB *class*. The OS ships a generic
HID driver, so any compliant HID device works without installing anything. A HID
device provides a **report descriptor**: a compact, byte-coded grammar that tells
the host the exact shape of the data it will send — e.g. "8 axes of 16 bits, then a
POV hat, then 128 buttons of 1 bit each." The host parses that and exposes the
controls through its joystick API.

> **Why descriptor size is load-bearing.** The report descriptor's declared length
> (`wDescriptorLength`) must match the real content exactly. The F411 bootloader bug
> (#2) was a descriptor array padded with trailing zeros; Windows parsed the zeros
> as malformed HID items and rejected the device with "Code 10." The fix pinned the
> array size with a `_Static_assert`. Lesson: in HID, declared sizes are part of the
> contract.

## Report IDs — many channels over one device

FreeJoy multiplexes several message types over the same HID interface using
**report IDs** (the first byte of each report selects its kind):

| Purpose | Direction | Payload |
|---|---|---|
| Joystick state | device → host | `joy_report_t` (axes, POVs, buttons) |
| Live parameters | device → host | `params_report_t` (diagnostics for the configurator) |
| Config read/write | both | chunks of `dev_config_t` |
| Firmware / DFU | both | firmware image chunks (bootloader uses REPORT_ID 4) |

(The application and bootloader use *different* report IDs for the firmware channel
for historical reasons — the configurator picks the right one per device.)

## The two key report structs

From `common_types.h`:

```c
typedef struct {                          // what the OS sees as the joystick
    analog_data_t  axis_data[MAX_AXIS_NUM];        // the 8 axes
    uint8_t        pov_data[MAX_POVS_NUM];         // POV hat angles
    uint8_t        button_data[MAX_BUTTONS_NUM/8]; // 128 buttons, 1 bit each
} joy_report_t;

typedef struct {                          // live diagnostics for the configurator
    uint16_t       firmware_version;       // wire-format compat token
    uint8_t        board_id, reserved_layout;
    analog_data_t  raw_axis_data[MAX_AXIS_NUM];    // pre-processing values
    analog_data_t  axis_data[MAX_AXIS_NUM];        // post-processing values
    uint8_t        phy_button_data[MAX_BUTTONS_NUM/8]; // raw physical buttons
    uint8_t        log_button_data[MAX_BUTTONS_NUM/8]; // resolved logical buttons
    uint8_t        shift_button_data;
    uint8_t        freejoyx_version_major/minor/patch; // human-readable version
} params_report_t;
```

- `joy_report_t` is the product: it's built from `out_axis_data[]`
  ([Layer 5](05-axes.md)) and the resolved logical buttons ([Layer 6](06-buttons.md)),
  sent every `exchange_period_ms`.
- `params_report_t` is the *introspection* channel. The configurator polls it to
  draw live axis bars, light up pressed buttons, show the firmware version in the
  sidebar, and detect physical presses for the Sequential-Assign feature. The
  `raw` vs processed axis arrays let it show "before/after the pipeline."

## Two USB stacks (one per chip)

- **F103** uses ST's older *USB FS Device* library (`application/Src/usb_*.c`:
  `usb_endp.c`, `usb_prop.c`, `usb_pwr.c`, …). Endpoint callbacks like
  `EP1_OUT_Callback` handle incoming reports.
- **F411** uses ST's *Cube USBD* CustomHID class (`usbd_*`), driven by `HAL_PCD`.

Both expose the *same* HID interface to the host (same VID:PID, same report shapes),
so the configurator and the OS can't tell which chip they're talking to from the USB
side alone. See [Layer 12](12-f103-vs-f411.md).

---
Next: [Layer 9 — Configuration model](09-configuration.md)
