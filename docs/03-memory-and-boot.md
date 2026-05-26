# Layer 3 — Memory layout & boot sequence

This is one of the most important architectural ideas in the project: the flash is
not one program, it's **three regions**, and a small program decides which one runs.

## Flash is carved into three regions

```
F411 (BlackPill) layout — from bootloader/f411/Src/main.c:
  0x08000000   Bootloader      (16 KB)    tiny; rarely changes
  0x08010000   Config storage  (64 KB)    your settings; survives reflash
  0x08020000   Application     (384 KB)   the actual FreeJoy firmware

F103 (BluePill) layout — analogous, smaller:
  0x08000000   Bootloader      (8 KB)
  ...          Config storage
  ...          Application
```

Why split it up?

- The **application** is the big program that does the joystick work. You update it
  often (every firmware release).
- A running program can't easily rewrite the flash it's *executing from*. So you
  need a separate, small program to receive and write a new application. That's the
  **bootloader**.
- **Config storage** is its own region so that reflashing the application doesn't
  erase your button mappings and calibration.

## The boot sequence

On power-up, the **bootloader runs first** (it lives at the chip's reset address,
`0x08000000`). Its only job is to decide: *run the app, or stay in flash-receive
(DFU) mode?*

From `bootloader/f411/Src/main.c`:

```c
magic  = Boot_GetMagicWord();   // did the running app ask us to enter DFU? (a value
                                //   stashed in a backup register, then cleared)
app_ok = Boot_AppValid();       // does the app's first vector-table word look like
                                //   a valid stack pointer in SRAM?
if (magic != BOOT_DFU_MAGIC && app_ok)
    Boot_EnterApp();            // hand off to the application
// else: bring up USB and wait to receive a new application over HID
```

Two independent signals decide:

1. **The magic word** — when the application wants to be reflashed, it writes a
   known value (`0x424C`) into a battery-backed register and resets. The bootloader
   sees it, clears it (so it's single-shot), and stays in DFU mode.
2. **App validity** — even with no magic word, if the application slot is blank or
   corrupt (its first word isn't a plausible stack pointer), the bootloader refuses
   to jump and stays in DFU so you can recover.

## The warm jump (`Boot_EnterApp`)

Handing control to the application is not a function call — it's a **warm jump**:

```c
WRITE_REG(SCB->VTOR, APP_VTOR_ADDR);          // point the CPU's vector table at the app
__ASM volatile ("MSR msp, %0" :: "r"(app_sp));// load the app's initial stack pointer
((funct_ptr)app_pc)();                        // branch into the app's reset handler
```

`VTOR` is the **Vector Table Offset Register** — it tells the CPU where the table
of interrupt handlers lives. Because the app is not at the chip's default address,
the app *also* re-points `VTOR` at itself early in its own startup
(`Board_RelocateVectorTable()`, called first thing in the application's `main()`).
If either side gets `VTOR` wrong, interrupts jump into the wrong code.

> **Case study — issue #36.** A warm jump doesn't reset the USB peripheral, so the
> host can still think the old device is attached. The fix was to cleanly disconnect
> USB (pull D+ low / `USBD_Stop`) and wait before the jump, so the host drops the
> old handle and re-enumerates the freshly-flashed app. See
> [Layer 11](11-flashing-and-dfu.md).

## Going the other way: app → bootloader

When you click "flash" in the configurator, the running **application** receives a
command, then deliberately hands control *back* to the bootloader. In
`application/Src/main.c`:

```c
if (bootloader > 0) {
    Board_TickStop();      // stop generating HID reports
    Delay_ms(50);          // let the last USB transmission finish
    Board_USB_DeInit();    // graceful USB disconnect
    Delay_ms(500);
    Board_EnterDfu();      // set the magic word + reset into the bootloader
}
```

So the bootloader↔application handoff is bidirectional, and both directions take
care to tear down USB cleanly first.

---
Next: [Layer 4 — The runtime model](04-runtime-model.md)
