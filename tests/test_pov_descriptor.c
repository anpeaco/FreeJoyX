/* Host logic-level test for the joystick HID report descriptor's POV (Hat
 * Switch) declaration -- regression guard for anpeaco/FreeJoyX#81
 * ("POV Hat detected internally but never reported to Windows").
 *
 * Compiles the REAL descriptor builder (application/Src/joy_report_desc.c) on a
 * desktop toolchain against the real common_types.h / common_defines.h and the
 * tests/stubs periphery.h, builds a descriptor for a POV-bearing config, parses
 * it as a HID report descriptor, and asserts the Hat Switch main item is
 * spec-conformant for the neutral value the firmware actually emits.
 *
 * The bug: application/Src/buttons.c emits POV_CENTER_VALUE (0xFF) at rest, but
 * the Hat Switch INPUT item is declared 0x81,0x02 (Data,Var,Abs) with Logical
 * range [0,7] and NO Null State flag. Per USB HID spec 6.2.2.5 a control that
 * reports an out-of-range rest value MUST advertise Null State (0x81,0x42),
 * otherwise strict HID hosts have no conformant way to see "centered" and drop
 * the hat entirely. Lenient DirectInput paths treat out-of-range as centered
 * and happen to work -- which is why the hat works on some setups and not
 * others.
 *
 * Build + run (from the FreeJoyX repo root), e.g. with the Qt MinGW gcc:
 *   gcc -std=c11 -I tests/stubs -I application/Inc \
 *       tests/test_pov_descriptor.c application/Src/joy_report_desc.c \
 *       -o build/test_pov_descriptor
 *   ./build/test_pov_descriptor      # exit 0 = pass
 */
#include <stdio.h>
#include <string.h>

#include "common_types.h"
#include "common_defines.h"
#include "joy_report_desc.h"

/* Neutral/centered POV value emitted by POVsGet's convert switch
 * (application/Src/buttons.c, "default:" arm). If that constant ever changes,
 * update it here -- the whole point of this test is that it is consistent with
 * the descriptor. */
#define POV_CENTER_VALUE 0xFF

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ---- minimal HID report-descriptor parser: locate the Hat Switch INPUT ---- */
typedef struct { int found; uint8_t flags; long lmin, lmax, rsize; } hat_item_t;

static hat_item_t find_hat_input(const uint8_t *d, uint16_t n)
{
    hat_item_t h; memset(&h, 0, sizeof(h));
    long lmin = 0, lmax = 0, rsize = 0, usage = -1;
    uint16_t i = 0;
    while (i < n) {
        uint8_t b0 = d[i];
        uint8_t sz = b0 & 0x03; if (sz == 3) sz = 4;
        uint8_t tag = b0 & 0xFC;
        long val = 0;
        for (int k = 0; k < sz; k++) val |= (long)d[i + 1 + k] << (8 * k);
        switch (tag) {
            case 0x14: lmin  = val; break;   /* Logical Minimum */
            case 0x24: lmax  = val; break;   /* Logical Maximum */
            case 0x74: rsize = val; break;   /* Report Size    */
            case 0x08: usage = val; break;   /* Usage          */
            case 0x80:                       /* INPUT (main item) */
                if (usage == 0x39 && !h.found) {   /* Hat Switch */
                    h.found = 1; h.flags = (uint8_t)val;
                    h.lmin = lmin; h.lmax = lmax; h.rsize = rsize;
                }
                usage = -1;
                break;
            default: break;
        }
        i += 1 + sz;
    }
    return h;
}

int main(void)
{
    /* Reporter's config shape: 24 buttons (74HC165), 7 analog axes, 1 POV hat. */
    app_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.buttons_cnt = 24;
    cfg.axis        = 0x7F;   /* X Y Z Rx Ry Rz Slider */
    cfg.axis_cnt    = 7;
    cfg.pov         = 0x01;
    cfg.pov_cnt     = 1;

    uint8_t desc[JOY_REPORT_DESC_MAX_SIZE];
    uint16_t len = BuildJoyReportDesc(desc, &cfg);
    CHECK(len > 0 && len <= JOY_REPORT_DESC_MAX_SIZE);

    hat_item_t h = find_hat_input(desc, len);
    CHECK(h.found);                 /* descriptor must declare a Hat Switch */
    CHECK(h.lmin == 0);
    CHECK(h.lmax == 7);
    CHECK(h.rsize == 8);

    /* The neutral value the firmware sends at rest. */
    int in_range   = (POV_CENTER_VALUE >= h.lmin && POV_CENTER_VALUE <= h.lmax);
    int null_state = (h.flags & 0x40) != 0;

    printf("Hat Switch INPUT: flags=0x%02X LogicalMin=%ld LogicalMax=%ld ReportSize=%ld\n",
           h.flags, h.lmin, h.lmax, h.rsize);
    printf("neutral value 0x%02X in [%ld,%ld]? %s ; Null State advertised? %s\n",
           POV_CENTER_VALUE, h.lmin, h.lmax, in_range ? "yes" : "no",
           null_state ? "yes" : "no");

    /* CONFORMANCE (USB HID spec 6.2.2.5): a control whose rest value is outside
     * its logical range MUST advertise Null State. */
    CHECK(in_range || null_state);

    printf(fails ? "\n%d CHECK(S) FAILED\n" : "ALL POV-DESCRIPTOR TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
