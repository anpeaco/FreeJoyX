# Layer 5 — Axes: the analog pipeline

An "axis" is a continuous value (a throttle, a stick direction, a pedal). This is
the most subtle subsystem, and the one most worth understanding deeply. All of it
lives in `application/Src/analog.c`, driven by `axis_config_t` (in
`common_types.h`).

## The internal representation

Every axis, regardless of source, is processed as a signed 16-bit value:

```
AXIS_MIN_VALUE = -32767   ...   AXIS_CENTER_VALUE = 0   ...   AXIS_MAX_VALUE = +32767
```

`typedef int16_t analog_data_t;`. Sources with different native ranges (ADC 0–4095,
ADS1115 0–32767, hall sensors ±180°) are first mapped into this common range, then
all later stages operate on it uniformly.

## The pipeline (order matters!)

`AxesProcess()` runs this per axis, every tick. The **order is the design** —
several real bugs have been ordering/reference-frame mistakes.

```
1. ACQUIRE   read source -> raw_axis_data[i]        (ADC / SPI / I2C / encoder)
2. MAP       native range -> -32767..32767          (map2 / map_tle)
3. FILTER    FIR low-pass smoothing                 (Filter)
4. SCALE     calibration min/center/max + deadband  (map3)
   4b. DYNAMIC DEADBAND (optional)                  (IsDynamicDeadbandHolding)
5. SHAPE     response curve                          (ShapeFunc)
6. RESOLUTION  optionally reduce bit depth           (SetResolutioin)
7. INVERT    flip sign if configured
8. PRESCALE  scale by a percentage (optionally button-gated)
9. TRIM/BUTTONS  increment/decrement/center via buttons
10. MULTI-AXIS  combine with another axis (sum/diff/equal)  -> scaled_axis_data[i]
```

### Stage 2 — `map2()`: linear range remap

`map2(x, in_min, in_max, out_min, out_max)` clamps to the input range, then linearly
rescales. Used to turn ADC 0–4095 (or ADS1115 0–32767) into −32767…32767.

### Stage 3 — `Filter()`: noise smoothing

A FIR (finite impulse response) filter: a weighted blend of the newest sample and a
history buffer. Higher levels (1–7) smooth more but add lag. Cheap pots are noisy;
this trades latency for steadiness.

### Stage 4 — `map3()`: calibration + static deadband

`map3()` is the heart of calibration. It maps your measured `calib_min /
calib_center / calib_max` onto `−32767 / 0 / +32767`, with a **dead zone** of
`deadband_size` around the center so a slightly-off-center stick reads as exactly
centered. Below the center it scales the lower half; above, the upper half. This is
**rescaling** — it stretches your used travel across the full output range.

### Stage 4b — dynamic deadband

Instead of a fixed center dead zone, dynamic deadband watches the *statistical
variance* of recent samples (`IsDynamicDeadbandHolding`) and **freezes** the output
when the signal is only jittering, not really moving. If `is_dynamic_deadband` is
set, `map3()` is called with deadband 0 and this logic runs after it.

> **Case study — the inverted-axis bug (upstream #255, our PR #47).** The hold
> check compared the freshly-scaled (pre-inversion) value against
> `scaled_axis_data[i]`, which stores the *previous frame's post-inversion* output.
> On an inverted axis the two are opposite-signed, so the difference is ~2× the
> value and always exceeds the threshold — the hold never fires and dynamic
> deadband silently does nothing. The fix tracks a dedicated **pre-inversion**
> reference (`deadband_ref_data[]`). This is a perfect illustration of why
> *which pipeline stage you take your reference from* matters.

### Stage 5 — `ShapeFunc()`: response curve

An 11-point curve (`curve_shape[11]`) lets you make the response non-linear — e.g.
fine control near center, faster toward the extremes. If the curve is the default
straight line, this stage is a no-op.

### Stages 7–10 — invert, prescale, trim, combine

- **Invert**: `tmp = -tmp`.
- **Prescale**: scale by `prescaler` percent, optionally only while a configured
  button is held (`AXIS_BUTTON_PRESCALER_EN`).
- **Trim / buttons**: buttons can increment, decrement, reset, or re-center an axis
  (`axis_trim_value[]`), with auto-repeat timed off `GetMillis()`.
- **Multi-axis**: `FUNCTION_PLUS/MINUS/EQUAL` combine this axis with
  `source_secondary` (e.g. sum two pedals into one axis).

The final value lands in `scaled_axis_data[i]`, and (if `out_enabled`) in
`out_axis_data[i]` which feeds the HID report.

## Axis sources

`source_main` in `axis_config_t` selects where the raw value comes from. It's a
signed value: `>= 0` means a pin index (internal ADC or an SPI sensor on that CS
pin); negative values are special sources:

```c
SOURCE_ENCODER = -3   // an encoder count acts as an axis
SOURCE_I2C     = -2   // an I2C sensor (ADS1115, AS5600)
SOURCE_NO      = -1   // unused
```

`AxesInit()` walks the pin map, identifies each configured sensor type (ANALOG,
TLE5011/5012, MCP320x, MLX90363/90393, AS5048A, ADS1115, AS5600), and sets up the
ADC ranking / sensor drivers accordingly. `AxesProcess()` then dispatches per source
type to read the raw value before the common pipeline.

## Where to read next in the code

- `analog.c::AxesProcess` — the pipeline above, end to end.
- `analog.c::map2 / map3 / ShapeFunc / Filter / IsDynamicDeadbandHolding` — each
  stage in isolation.
- `common_types.h::axis_config_t` — every per-axis setting and how it's bit-packed.

---
Next: [Layer 6 — Buttons](06-buttons.md)
