# FreeJoyX Style Guide

This document captures the conventions for code in **this fork** of FreeJoy.
The aim is to keep new code consistent with the inherited upstream style so
the diff against `github.com/FreeJoy-Team/FreeJoy` stays small and merges
remain tractable.

## Guiding principle

> **Do not bulk-reformat upstream code.** This repo is a downstream fork.
> Wholesale style changes against files we did not author create perpetual
> merge friction for no functional gain. Touch existing files only when
> extending them, and then match the surrounding style.

`.clang-format` and `.editorconfig` exist to tell editors and new
contributions what the *target* shape is — not as enforcement on the
existing tree.

## Formatting

| | |
|---|---|
| Indent | Tabs, 4 columns wide |
| Braces (functions, control flow) | Allman — opening brace on its own line |
| Braces (typedefs, structs, enums, initializers) | K&R — opening brace inline |
| Pointer alignment | Middle (`dev_config_t * p_dev_config`) — matches upstream |
| Space before open paren | Always — including function names: `void ButtonsDebounceProcess (...)` |
| Column limit | 100 (soft); tabular data may exceed |
| Trailing newline | Required |
| Line endings | LF |

Example:

```c
void ButtonsDebounceProcess (dev_config_t * p_dev_config)
{
    int32_t  millis;
    uint16_t debounce;

    millis = GetMillis();
    for (uint8_t i = 0; i < MAX_BUTTONS_NUM; i++)
    {
        if (a2b_first != a2b_last && i > a2b_first && i <= a2b_last)
        {
            debounce = p_dev_config->a2b_debounce_ms;
        }
        else
        {
            debounce = p_dev_config->button_debounce_ms;
        }
    }
}
```

## Naming

| Construct | Convention | Example |
|---|---|---|
| Functions | `PascalCase` for public API; `snake_case` is also accepted in legacy code | `ButtonsDebounceProcess`, `GetMillis` |
| Static helpers | `PascalCase` matches surrounding API; OK to use `snake_case` in new files for clarity | `GestureClaimedSweep` |
| Variables, struct fields | `snake_case` | `debounce`, `physical_buttons_state` |
| Typedefs | `snake_case_t` suffix | `dev_config_t`, `physical_buttons_state_t` |
| Macros, enum values | `UPPER_SNAKE_CASE` | `MAX_BUTTONS_NUM`, `BUTTON_NORMAL` |
| File-scope statics | `snake_case` (no leading underscore — collides with the toolchain reserved namespace) | `gesture_claimed[]` |

If a new function clearly belongs to a module (`Buttons*`, `Encoder*`,
`Analog*`), use the module's existing case style. Don't introduce a third.

## Headers

Every `.c` and `.h` file inherits the upstream block-comment header with
the GPL notice. Do not strip it. New files should reproduce it; new
*sections* in existing files don't need a fresh banner.

For exported APIs, use Doxygen-style block comments:

```c
/**
 * @brief  Processing debounce for raw buttons input.
 * @param  p_dev_config: Pointer to device configuration
 * @retval None
 */
```

`@brief / @param / @retval` is the inherited triple. Keep it. Don't
introduce `///` line-doc style.

For internal statics, a one-line `//` comment above the definition is
enough.

## Headers and include guards

Use the `#ifndef FILENAME_H` / `#define FILENAME_H` / `#endif // FILENAME_H`
pattern. Do **not** use `#pragma once` — it's not used anywhere in this
codebase, and mixing the two is the kind of inconsistency this guide
exists to prevent.

## Wire-format changes — the lockstep rule

Any change to `dev_config_t`, `params_report_t`, or related shared types
must move **four items in lockstep**:

1. `FIRMWARE_VERSION` in `application/Inc/common_defines.h` — must cross
   the `& 0xFFF0` mask boundary so devices factory-reset. (See
   `memory/feedback_firmware_version_bumps.md` in the project area.)
2. `FREEJOY_DEV_CONFIG_SIZE` and `FREEJOY_PARAMS_REPORT_SIZE` constants
   in `application/Inc/common_defines.h` — the `_Static_assert` lines
   at the bottom of `common_types.h` will fail the build if you bump
   the struct shape without bumping the constant.
3. The legacy archive entry in the configurator's
   `src/legacy/legacy_types.h` plus a migrator in `legacy_migrator.cpp`
   wired into `migrateLegacyConfig()`'s dispatch.
4. The synced copy of both header files in the configurator repo —
   `common_types.h` and `common_defines.h` are duplicated by manual
   sync, not symlinked.

Forgetting any of these four leaves boards in the field unmigratable
or silently corrupted on flash. There is no safety net beyond the static
asserts.

## Compiler warnings

Current build flags are `-O2 -Wall`. Newer code should be clean under
`-Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes`. These flags are
**not enabled by default** because turning them on retroactively floods
the build with warnings from upstream code that we don't want to touch.

When extending a module, consider building it with extra warnings
locally:

```sh
make TARGET=f103 EXTRA_CFLAGS="-Wextra -Wshadow"
```

and address warnings in the code you've actually authored.

`-Werror` is intentionally not used. The build needs to remain green
even when upstream introduces a warning we haven't yet absorbed.

## Comments

- Explain **why**, not what. `// increment counter` is noise; `// claim
  the gesture so a sister NORMAL slot suppresses its delayed press fire`
  is signal.
- Don't leave commented-out code. If it's worth keeping, write it as a
  comment explaining what was tried and why it was reverted; otherwise
  delete it.
- Don't mix English with other natural languages. The codebase is
  English-only — match that.

## Relationship to the configurator

Two headers must stay byte-identical between this repo and
`FreeJoyXConfiguratorQt`:

- `application/Inc/common_types.h`   ↔ `src/common_types.h`
- `application/Inc/common_defines.h` ↔ `src/common_defines.h`

There is no submodule. Diff them manually before committing any change
to either side. The static-assert at the bottom of `common_types.h`
will catch struct-size drift but not field-name drift.

## What this guide is not

It's not a CI gate. It's not run pre-commit. It exists so a contributor
joining the project tomorrow can read one file and write code that
matches the rest. If something here disagrees with the existing code
in a load-bearing way, the existing code wins until this guide is
updated.
