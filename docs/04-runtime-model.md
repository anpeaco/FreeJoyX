# Layer 4 — The runtime model (setup + superloop)

There is no operating system. The application does a one-time **setup**, then runs
a **superloop** forever. Some work is offloaded to **timer interrupts**. This layer
walks the real `application/Src/main.c`.

## Setup: `main()` before the loop

In order (abridged from `main.c`):

```c
Board_RelocateVectorTable();      // 1. point VTOR at the app (see Layer 3)
SysTick_Init();                   // 2. start the millisecond time base (GetMillis)
DevConfigGet(&dev_config);        // 3. load saved config from flash into RAM

if ((dev_config.firmware_version & 0xFFF0) != (FIRMWARE_VERSION & 0xFFF0)) {
    DevConfigSet(&init_config);   // 4. first boot / format mismatch -> factory defaults
    DevConfigGet(&dev_config);
}
AppConfigInit(&dev_config);       // 5. derive counts (how many axes, buttons, ...)

Board_USB_Init();                 // 6. bring up USB; enumerate to the host
Delay_ms(1000);

IO_Init(&dev_config);             // 7. configure pins per the config
EncodersInit(...); ShiftRegistersInit(...);
RadioButtons_Init(...); SequentialButtons_Init(...); Gestures_Init(...);
AxesInit(&dev_config);            // 8. configure ADC/DMA + sensor drivers
Timers_Init(&dev_config);         // 9. start the periodic peripheral-reading tick
```

Key points:

- **Step 4** is the wire-format guard. `firmware_version & 0xFFF0` is the *format
  generation*; if the stored config's generation differs from this firmware's, the
  config is incompatible and the device factory-resets. See
  [Layer 10](10-wire-format-and-versioning.md).
- **Step 5** turns raw config into derived runtime facts (`app_config_t`: axis
  count, button count, POV count, …) used to build the HID report.
- **Step 9** starts a timer that periodically reads slow sensors and drives the HID
  report cadence (`exchange_period_ms`), independent of the main loop's speed.

## The superloop

```c
while (1) {
    ButtonsDebounceProcess(&dev_config);  // sample physical buttons, debounce
    ButtonsReadLogical(&dev_config);      // resolve logical buttons (types, gestures)

    LEDs_PhysicalProcess(&dev_config);    // matrix/single LED output

    analog_data_t tmp[8];
    AnalogGet(NULL, tmp, NULL);           // fetch latest processed axis values
    PWM_SetFromAxis(&dev_config, tmp);    // drive PWM LEDs from an axis if configured

    ArgbLed_Process(&dev_config, ...);    // addressable RGB (WS2812B) effects

    if (bootloader > 0) { /* hand off to DFU -- see Layer 3 */ }
}
```

Notice what is **not** here: the heavy axis math (`AxesProcess`) and the USB report
transmit. Those run on the **timer tick**, not in the loop. The loop mostly handles
buttons and LEDs and watches for the "enter bootloader" flag.

## Two clocks of work: loop vs tick

| Runs in the superloop | Runs on the timer tick / interrupts |
|---|---|
| Button debounce & logical resolution | ADC conversion + `AxesProcess()` |
| LED output | Slow sensor reads (SPI/I2C) |
| Watching the `bootloader` flag | Building & sending the HID report |
| ARGB effects | USB packet servicing |

This split keeps timing-sensitive work (sensor sampling, USB cadence) on a steady
clock while the loop does the rest as fast as it can. `GetMillis()` (backed by
**SysTick**, started in step 2) is the shared time reference used everywhere for
debounce windows, gesture timing, trim auto-repeat, etc.

## The time base: SysTick and `GetMillis()`

`SysTick` is a core timer that fires every millisecond and increments a counter.
`GetMillis()` returns that counter. Almost every timed behaviour — button debounce
(`button_debounce_ms`), tap cutoff (`tap_cutoff_ms`), encoder press time, axis trim
auto-repeat — is "compare `GetMillis()` against a stored deadline." Getting SysTick
wrong (a bug family that bit during the F411 port) breaks all of it at once.

---
Next: [Layer 5 — Axes](05-axes.md)
