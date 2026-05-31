/**
  ******************************************************************************
  * @file           : image_id.h
  * @brief          : Compile-time identification footer for the application
  *                   image. Lets the configurator's flasher refuse to flash
  *                   a board-mismatched binary and display the target firmware
  *                   version in the pre-flash confirmation dialog.
  *                   Issue anpeaco/FreeJoyX#23.
  *
  *                   Firmware-only header -- not synced into the configurator
  *                   repo. The configurator carries its own parser-side copy
  *                   of the struct shape (slice 2,
  *                   anpeaco/FreeJoyXConfiguratorQt#15). The `magic` and
  *                   `struct_size` fields let the parser detect drift if the
  *                   two ever diverge.
  ******************************************************************************
  */

#ifndef __IMAGE_ID_H__
#define __IMAGE_ID_H__

#include <stdint.h>

/* 'FRJY' little-endian. Scanned for by the configurator to locate the
 * footer in the linked binary without depending on a fixed offset. */
#define FREEJOY_IMAGE_MAGIC                 0x46524A59u

typedef struct __attribute__((packed)) freejoy_image_id_t {
    uint32_t magic;        /* FREEJOY_IMAGE_MAGIC */
    uint16_t struct_size;  /* sizeof(freejoy_image_id_t); forward-compat marker */
    uint16_t board_id;     /* BOARD_ID_F103_BLUEPILL / BOARD_ID_F411_BLACKPILL */
    uint16_t fw_version;   /* mirrors FIRMWARE_VERSION (wire-format compat key) */
    uint16_t build_id;     /* mirrors FIRMWARE_BUILD_ID (8-bit counter widened) */
    /* Human semver (FREEJOYX_VERSION major.minor.patch). Added so the flasher
     * can show "0.1.3" for the selected binary instead of the wire-format token
     * fw_version ("0x0020"). Reclaimed from reserved[0] -- struct_size is
     * UNCHANGED (28), so older parsers that ignore the reserved tail keep
     * working and newer ones read the semver here. All-zero on pre-semver
     * binaries, which the configurator treats as "not present" and falls back
     * to fw_version. */
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  version_pad;  /* keep 4-byte alignment of the trailing reserved[] */
    uint32_t reserved[3];  /* zero-filled; reserved for future fields */
} freejoy_image_id_t;

extern const freejoy_image_id_t freejoy_image_id;

#endif /* __IMAGE_ID_H__ */
