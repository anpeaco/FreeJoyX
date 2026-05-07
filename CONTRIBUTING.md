# Contributing to FreeJoyX

This is a downstream fork of [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy).
The roadmap, design decisions, and scope are tracked in plan files at the
repo root and in the parent project workspace.

## Before you change code

1. Read **STYLE.md** — captures the inherited conventions and the rule
   that we don't bulk-reformat upstream files.
2. If you're touching `dev_config_t`, `params_report_t`, USB report
   payloads, or anything else on the wire between firmware and
   configurator, read **STYLE.md → "Wire-format changes — the lockstep
   rule"**. Skipping any of the four lockstep items leaves boards in
   the field broken.
3. Check the GitHub Issues on `anpeaco/FreeJoyX` and
   `anpeaco/FreeJoyConfiguratorQtX` for related work.

## Build

The firmware builds in MSYS2 MinGW64. From `armgcc/`:

```sh
make TARGET=f103          # BluePill (STM32F103C8T6)
make TARGET=f411          # BlackPill (STM32F411CEU6)
make install-firmware     # copies binaries to release/
make release RELEASE_VERSION=v1.7.8
```

The default target is F103. Both targets must build cleanly before any
PR lands.

## Commits

- One logical change per commit. Wire-format bumps and their migrators
  belong in the same commit (see lockstep rule).
- Reference issues with `Closes anpeaco/FreeJoyX#NN` in the commit
  trailer when the change resolves an issue.
- Keep commits buildable. If a wire-format change spans firmware and
  configurator, the firmware-side commit must still build the firmware
  on its own; the configurator-side commit must still build the
  configurator on its own.

## Pull requests

- Branch off `master`.
- Push the branch and open a PR; let CI / local-build pass before
  merging.
- Squash on merge if the branch contains fixup commits; otherwise
  preserve the commit boundary so the wire-format / migrator pairing
  stays visible in history.

## Reporting bugs

Open an issue on `anpeaco/FreeJoyX`. Include:

- Board (BluePill / BlackPill / other)
- Firmware version (`FIRMWARE_VERSION` from the device, e.g. `0x1780`)
- Configurator version
- Reproduction steps and expected vs. actual behavior

Hardware-specific issues (USB enumeration failures, flashing failures)
should also list the OS and host USB stack.
