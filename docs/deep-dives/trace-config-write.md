# Deep dive — Trace: a config write, end to end

Follow a "Write config to device" click from the configurator to the chip's flash.
This ties together Layers 8, 9, and 10.

## 0. In the configurator (host side)

You edit settings in the GUI; they live in an in-memory `dev_config_t` mirror. You
click **Write Config**. The configurator:

1. (Optionally) backs up the device's current config first.
2. Serialises its `dev_config_t` to the exact wire-format bytes.
3. Splits the bytes into HID-report-sized **chunks**.

## 1. Validation handshake

Before writing, the firmware guards two things (see [Layer 9](../09-configuration.md)
and [Layer 10](../10-wire-format-and-versioning.md)):

- **Wire-format generation**: if `incoming.firmware_version & 0xFFF0` != this
  firmware's, reply `0xFE` ("reflash the firmware") and abort.
- **board_id**: if the config was saved for a different board, reply `0xFD` ("wrong
  board; convert it"). The configurator then offers the cross-board converter.

If both pass, the firmware accepts the chunk stream.

## 2. Reassembly in RAM

The firmware receives the chunks (over the config report ID), writing them into a
RAM buffer until the full `dev_config_t` is assembled. Reassembly errors (size
mismatch) are rejected here — the compile-time `_Static_assert` on struct size
guarantees both sides agree on how many bytes that is.

## 3. Commit to flash — `DevConfigSet()`

`config.c::DevConfigSet()` writes the assembled struct into the config-storage flash
region:

```
for each page in the config region:
    if (ConfigFlash_ErasePage(addr) != OK) return -1;   // abort on erase failure
for each word of the new config:
    if (ConfigFlash_WriteWord(addr, word) != OK) return -1;
```

Why the careful error handling (issue #3): flash bits can only go 1→0 without an
erase. If an erase silently failed and you programmed anyway, you'd AND the new data
with stale contents — silent corruption. So every step is checked and the whole
operation aborts on the first failure, leaving the device alive to retry rather than
half-written. On F4 there's also a data-cache flush consideration around the write.

## 4. Apply: reset

A successful write is followed by a reset so the new config takes effect cleanly
(the app re-runs `DevConfigGet()` at boot — [Layer 4](../04-runtime-model.md)). If
the write *failed*, the firmware skips the reset and stays running so you can retry,
rather than rebooting into a half-config / version-mismatch loop.

## 5. Reconnect

After the reset the device re-enumerates. The configurator's reconnect logic
(identity match on serial / VID+PID / path) reattaches, reads the config back, and
confirms the round-trip. A post-write read-back that doesn't match is how the
configurator detects a partial/failed write.

## Failure modes worth knowing

- **Wrong generation** → `0xFE`, no write. Action: reflash firmware.
- **Wrong board** → `0xFD`, no write. Action: accept cross-board conversion.
- **Erase/program failure** → `DevConfigSet` returns −1, no reset, device stays up.
- **Reset-but-no-reconnect** → handled by the configurator's post-write watchdog
  (and, in the flasher, the stuck-device recovery from issue #36).

---
See also: [Trace: a button press](trace-button-press.md)
