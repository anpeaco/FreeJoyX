# External ADC Axis Auto-Detect — Plan

Extend the existing rotate-to-detect feature to external SPI ADCs
(`MCP3201/3202/3204/3208`) so the configurator can identify an
unmapped external-ADC channel by movement, the same way it already
does for the internal ADC pins `PA0–PA7`.

Sibling to the MCP23S17 work — both touch the `sensors[]` allocation
and the SPI dispatch path.

## Background — how internal-ADC detect works today

- `params_report_t.detect_axis_raw[MAX_AXIS_NUM]` (`application/Inc/common_types.h`)
  carries a raw value per internal analog pin index (`PA0–PA7`),
  `AXIS_MIN_VALUE` for unsampled pins.
- Populated by `AnalogGetDetect()` at the end of `application/Src/analog.c`,
  called once per report tick from `Board_TickISR` (`application/Src/usb_app.c`).
- It works unconditionally because the internal ADC is an always-on
  parallel DMA scan: `AxesInit` ranks **every** pin tagged `AXIS_ANALOG`
  (mapped or not) into `analog_rank_pin[]`/`analog_rank_cnt`, and the ADC
  regular-sequence build scans all of them into `adc_data[]`.
  `AnalogGetDetect` just rescales that buffer.

## Why external ADC is excluded today

External sensors are **axis-bound**: `AxesInit` only allocates a
`sensors[]` slot when some axis's `axis_config_t.source_main` points at
that device's CS pin, and the SPI DMA chain (`Sensor_OnSpiRxComplete`,
`application/Src/sensor_dispatch.c`) reads only the channels that mapped
axes address via their `channel` field. An unmapped MCP channel is never
sampled, so there is no raw value to report.

## Design decision

A configured `MCP320x_CS` pin is an explicit statement of intent: the
user wired the chip and declared it. Therefore we scan **all** channels
of any configured MCP320x device every tick, regardless of axis mapping.
No "detect mode" flag.

Consequences:
- Detect data falls out of the normal scan for free — mapped axes and
  `detect_ext_raw[]` read the same `sensors[].data[]` buffer.
- Per-tick SPI cost = sum of channels across all configured MCP devices
  (one MCP3208 = 8 reads/tick, trivial with DMA; self-limited by the
  configurator's sensor cap). No gating required.

## Firmware changes (`anpeaco/freejoyx`)

1. **Decouple sensor allocation from axis mapping** — `AxesInit`
   (`application/Src/analog.c`): allocate a `sensors[]` slot for any pin
   tagged `MCP3201_CS/3202_CS/3204_CS/3208_CS`, even when no axis sources
   it. Record the device channel count by variant (3201=1, 3202=2,
   3204=4, 3208=8).
2. **Scan full channel count** — `Sensor_OnSpiRxComplete`
   (`application/Src/sensor_dispatch.c`) and the SPI kickoff in
   `Board_TickISR` (`application/Src/usb_app.c`): iterate the device's
   full channel count unconditionally, storing every channel into
   `sensors[].data[]`. (`sensor_t.data[24]` already holds 8×3 bytes — no
   buffer resize.)
3. **New populator** — add `AnalogGetDetectExt()` mirroring
   `AnalogGetDetect()`: for each configured MCP320x device/channel call
   `MCP320x_GetData(&sensor, ch)` and rescale to axis range; call it next
   to `AnalogGetDetect` in `Board_TickISR`.
4. **Report field** — `params_report_t` (`application/Inc/common_types.h`):
   append `analog_data_t detect_ext_raw[N]` at the **end** of the struct
   to preserve wire compatibility. Bump `FIRMWARE_VERSION` / params report
   size and update the `_Static_assert`. (`N` = max external channels to
   expose; size against the configurator's sensor/axis caps.)

## Configurator changes (`anpeaco/freejoyxconfiguratorqt`)

Kept in lockstep per the wire-format rule (`common_types.h` /
`common_defines.h` mirror; see `header-sync.yml`).

- Mirror the `detect_ext_raw[]` field addition in the configurator's
  `common_types.h`.
- Extend the rotate-to-detect UI so external-ADC channels are surfaced
  alongside the `PA0–PA7` internal pins, keyed by (CS pin, channel).
- On detect, offer to bind the identified channel to a logical axis
  (set `source_main` = the CS pin index, `channel` = detected channel).

## Wire-format / compatibility

- Field is **appended**, so older configs remain readable and
  forward-migratable (matches the README's generation rules).
- Bump `FIRMWARE_VERSION` within the current `0x0020` generation as
  appropriate; verify the `_Static_assert` on report size passes for both
  `f103` and `f411` builds.

## Test plan

- Configure an MCP3208 CS pin with **no** axis mapped; confirm all 8
  channels report live raw values in `detect_ext_raw[]`.
- Rotate a pot on an unmapped channel; confirm the configurator
  identifies it.
- Map a channel to an axis; confirm normal axis output still works and
  reads the same buffer.
- Verify no regression in internal-ADC detect (`PA0–PA7`).
- Build both `make TARGET=f103` and `make TARGET=f411`.

## Notes

- Symbol/file references above are from a source read of `master`
  (`analog.c`, `mcp320x.c`, `sensor_dispatch.c`, `usb_app.c`,
  `common_types.h`, `board/f411_blackpill/Src/board_spi.c`); confirm exact
  names/line numbers at implementation time.
- F411 external-ADC SPI bus is **SPI1 / AF5: SCK=PB3, MISO=PB4,
  MOSI=PB5** (`board_spi.c`), CS is an ordinary GPIO toggled from
  `pin_config[]`. Relevant to the companion breakout-board layout.
