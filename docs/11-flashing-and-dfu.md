# Layer 11 — Flashing & DFU

"Flashing" = writing a new application image into the chip's flash. FreeJoy does this
over USB, with no external programmer, via the bootloader. DFU = Device Firmware
Update. This layer covers the protocol and its failure modes.

## The cast

- **Bootloader** — the small program at `0x08000000` ([Layer 3](03-memory-and-boot.md)).
  In DFU mode it brings up the *same* HID interface as the app and waits to receive
  an image. Code: `bootloader/Src/*` (F103), `bootloader/f411/Src/*` (F411).
- **Application** — when told to update, it hands control to the bootloader.
- **Configurator** — sends the new image. Its flasher logic drives the whole flow.

## Entering DFU

The application receives a "enter flasher" command and sets a flag; the superloop
then (`main.c`): stops HID ticks, disconnects USB, and calls `Board_EnterDfu()`,
which writes the **magic word** (`0x424C`) into a backup register and resets. On the
next boot the bootloader sees the magic and stays in DFU instead of jumping to the
app ([Layer 3](03-memory-and-boot.md)).

## The transfer protocol

The configurator streams the image as HID reports under REPORT_ID 4 (the
bootloader's firmware channel). From `bootloader/f411/Src/boot_usb_if.c`:

```
First packet [4, 0,0,0, len_lo, len_hi, crc_lo, crc_hi, ...]
    -> erase the application sectors
    -> reply [4, 0,1]  (send me chunk 1)
Body packet  [4, cnt_hi, cnt_lo, 0, <60 bytes>]
    -> program 60 bytes at APP_VTOR + (cnt-1)*60
    -> reply [4, (cnt+1)>>8, (cnt+1)&0xFF]  (send next)
Last packet  (cnt*60 >= len)
    -> program, then CRC16 the whole received image
    -> reply [4, 0xF0, 0x00] OK  /  0xF002 CRC error  / ...
```

Status codes: `0xF000` OK, `0xF001` size error, `0xF002` CRC error, `0xF003` erase
error, `0xF004` program error. The CRC check at the end guarantees the whole image
arrived intact before the bootloader will run it.

Notes you can see in the code:

- Each chunk is programmed **halfword-by-halfword** (the 60-byte chunk isn't
  word-aligned; halfword keeps the loop simple). Each program is checked; the loop
  aborts on the first failure (issue #3).
- The reply buffer is `static`, because on F411 the USB peripheral reads the source
  buffer asynchronously after the function returns (a stack buffer would be dead by
  then).
- The bootloader must call `USBD_CUSTOM_HID_ReceivePacket()` after each chunk to
  re-arm the OUT endpoint, or only the first chunk gets through.

## Finishing: bootloader → application

After a good CRC, the bootloader sets `flash_finished`. The main loop then tears down
USB and warm-jumps into the new application.

> **The stuck-device problem (issue #36).** Because the handoff is a warm jump (not
> a full reset), if USB isn't cleanly disconnected first, the host keeps the old
> device handle and the freshly-flashed app can't enumerate — the user has to
> physically unplug. The fix: both bootloaders pull D+ low / `USBD_Stop` and wait
> *before* jumping, so the host sees a clean DETACH and re-enumerates the new app.
> F103 does this in `USB_Shutdown()`; F411 via `USBD_Stop()` + `USBD_DeInit()`.

## Recovery

If the application slot is blank or invalid, the bootloader refuses to jump and
stays in DFU on its own ([Layer 3](03-memory-and-boot.md)) — so a bad flash is
recoverable by just reflashing, no programmer needed. As a last resort you can flash
the bootloader (or a full image) over SWD with an ST-Link + STM32CubeProgrammer.

## Where this is going

- **Guided flasher** (configurator, tracking issue): three-pane flow, compatibility
  check, progress dialog, auto-restore config across format bumps.
- **One-click upgrade** (configurator #9): snapshot → flash → migrate → write-back,
  with auto-rollback on failure.

---
Next: [Layer 12 — F103 vs F411](12-f103-vs-f411.md)
