# Layer 10 — Wire format & versioning

The firmware and the configurator are separate programs in separate repos, but they
exchange raw C structs (`dev_config_t`, `params_report_t`). The exact byte layout of
those structs is a **contract** — the "wire format." This layer explains how that
contract is versioned and defended, because most of the project's release discipline
revolves around it.

## The problem

If the firmware writes `dev_config_t` one way and the configurator reads it another
way — even a one-byte difference from a changed field or different struct padding —
you get silent corruption: settings land in the wrong place, axes go to extremes,
buttons map wrong. Nothing crashes; it just misbehaves. So the format must be
*locked* and any change must be *deliberate and detected*.

## `FIRMWARE_VERSION` and the generation mask

`FIRMWARE_VERSION` (in `common_defines.h`, e.g. `0x0020`) stamps the wire format.
The crucial trick is the **generation mask** `& 0xFFF0`:

- The high three nibbles (`0xFFF0`) are the **format generation**. If these change,
  the layout is considered incompatible.
- The low nibble is free for compatible tweaks.

Both sides compare with the mask:

- Firmware at boot (`main.c`): if the stored config's `firmware_version & 0xFFF0`
  differs from the firmware's, the config is from a different generation →
  **factory reset**.
- Firmware on a config write: generation mismatch → reject with `0xFE`.
- Configurator: same `& 0xFFF0` check before trusting a device's config.

So bumping the generation forces a clean reset rather than risking a misread.

## The compile-time trip-wire

You can't easily *see* a struct's byte size by eye, so the size is asserted at
compile time. In `common_types.h`:

```c
_Static_assert(sizeof(dev_config_t)    == FREEJOY_DEV_CONFIG_SIZE,    "...");
_Static_assert(sizeof(params_report_t) == FREEJOY_PARAMS_REPORT_SIZE, "...");
```

If anyone changes a field and the struct size drifts, **the build fails** with a
message telling you to bump `FIRMWARE_VERSION`, archive the old shape, and update
the size constant. This converts a silent runtime-corruption bug (issue #10) into a
loud compile error.

## The archival rule (when you DO change the format)

When a format change is intentional, the discipline is:

1. Bump `FIRMWARE_VERSION` generation (`& 0xFFF0` changes).
2. Update `FREEJOY_DEV_CONFIG_SIZE` / `FREEJOY_PARAMS_REPORT_SIZE`.
3. **Archive the old struct shape** in the configurator
   (`legacy_types.h` + `legacy_migrator.cpp`) so the configurator can still *read*
   old configs and migrate them forward (memcpy into the new shape + restamp the
   version, logging any semantic change).

This is how a user on an old version can upgrade without losing their config: the
configurator migrates it; the device factory-resets and the configurator writes the
migrated config back.

### Numerically stable enum extensions

Where possible, changes are made *additively* so they don't shift the format at
all: new enum values are **appended** (e.g. `POV3_CENTER`, `LOGIC`, `TAP`,
`DOUBLE_TAP`, `LOGIC_OP_XNOR` are all at the end of their enums). Old firmware
seeing a new value falls into a default/no-op case — a soft failure, not corruption.
Renames that keep the same integer value (LONG_PRESS → TAP) preserve decoding of old
configs while changing behaviour, which is why *that* kind of change still bumps the
generation (same bytes, different meaning) even though the layout is unchanged.

## Keeping the two repos in sync

`common_defines.h` and `common_types.h` are **physically duplicated** in the
firmware and the configurator. A CI job ("Compare common_*.h with configurator")
diffs them on every PR and fails if they drift, except inside explicit
`SYNC_SKIP_BEGIN/END` markers (used for firmware-only runtime fields like gesture
state that aren't part of the wire format). `FREEJOYX_VERSION` (the human-readable
project version) also lives in this synced header, so version bumps must land in
lockstep across both repos.

## Two version numbers, on purpose

- `FIRMWARE_VERSION` — the **wire-format compat token**. Changes only when the
  format generation changes.
- `FREEJOYX_VERSION` — the **human release version** (e.g. 0.1.2). Reported live in
  `params_report_t` so the configurator sidebar can show it, decoupled from compat.

Don't conflate them: you can ship many `FREEJOYX_VERSION` releases without touching
`FIRMWARE_VERSION`.

---
Next: [Layer 11 — Flashing & DFU](11-flashing-and-dfu.md)
