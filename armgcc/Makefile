# Top-level dispatcher. Pick the chip family with TARGET={f103|f411};
# defaults to f103 so a bare `make` keeps working as before.
TARGET ?= f103

# Wire-format version extracted from the firmware's common_defines.h
# header. Used to name versioned firmware artefacts so multiple builds
# can coexist in the configurator's firmware/ folder without colliding.
# FW_VERSION is the raw 4-hex-digit value (e.g. "1770"); FW_TAG is the
# user-facing rendering (e.g. "v1.7.7b0") that matches what the
# configurator shows in its Firmware row.
FW_VERSION := $(shell grep -E "^\#define FIRMWARE_VERSION" \
                       ../application/Inc/common_defines.h | \
                  awk '{print $$3}' | sed 's/0x//' | head -c 4)
FW_TAG := v$(shell echo $(FW_VERSION) | sed 's/\(.\)\(.\)\(.\)\(.\)/\1.\2.\3b\4/')

# RELEASE_VERSION (optional override). When unset, artefacts are named
# after FW_TAG so the filename matches the firmware version shown in
# the configurator's Firmware row -- e.g. FIRMWARE_VERSION 0x1770
# produces "freejoyx-f103-app-v1.7.7b0.bin", and the device's UI also
# reports v1.7.7b0. No conflicting numbering for users to reconcile.
#
# When set (typically to a GitHub release tag like v1.7.7b0 or a fix
# variant like v1.7.7b0-r1), artefacts are named after the override
# instead.
#
# Convention:
#   RELEASE_VERSION unset             -> freejoyx-f103-app-v1.7.7b0.bin
#                                        (matches UI; default for
#                                         dev builds and tagged releases
#                                         that don't need a suffix)
#   RELEASE_VERSION=v1.7.7b0-r1       -> freejoyx-f103-app-v1.7.7b0-r1.bin
#                                        (release-fix without a wire-
#                                         format bump -- bug-fix only)
RELEASE_VERSION ?=
NAME_VERSION := $(if $(RELEASE_VERSION),$(RELEASE_VERSION),$(FW_TAG))

# CONFX_FIRMWARE_DIR (optional): absolute path to the configurator's
# <exe>/firmware/ folder. When set, install-firmware copies versioned
# binaries there directly so the next configurator launch picks them
# up in the Source dropdown without manual copying. Otherwise drops
# them in build/$(TARGET)/install/ and the user moves them manually.
CONFX_FIRMWARE_DIR ?=
INSTALL_DIR := $(if $(CONFX_FIRMWARE_DIR),$(CONFX_FIRMWARE_DIR),build/$(TARGET)/install)

# Naming convention -- documented here so it stays consistent across
# the makefile, GitHub release uploads, and the configurator's
# FirmwareLibrary asset display:
#   freejoyx-<board>-<app|boot>-<version>.bin
NAME_APP  := freejoyx-$(TARGET)-app-$(NAME_VERSION).bin
NAME_BOOT := freejoyx-$(TARGET)-boot-$(NAME_VERSION).bin

all:
	$(MAKE) -f makefile.app TARGET=$(TARGET)
	$(MAKE) -f makefile.boot TARGET=$(TARGET)

app:
	$(MAKE) -f makefile.app TARGET=$(TARGET)

boot:
	$(MAKE) -f makefile.boot TARGET=$(TARGET)

# install-firmware: copy the build outputs to versioned filenames.
# Without arguments, lands them in build/$(TARGET)/install/ for the
# user to copy manually. With CONFX_FIRMWARE_DIR=<path>, drops them
# directly into the configurator's firmware/ folder.
#
# Usage:
#   make TARGET=f103 install-firmware
#   make TARGET=f103 install-firmware RELEASE_VERSION=v0.2.0
#   make TARGET=f411 install-firmware CONFX_FIRMWARE_DIR=/c/Users/.../FreeJoyConfiguratorQtX/build_phase4a/release/firmware
install-firmware: all
	mkdir -p $(INSTALL_DIR)
	cp build/$(TARGET)/app/FreeJoy.bin   $(INSTALL_DIR)/$(NAME_APP)
	cp build/$(TARGET)/boot/Bootloader.bin $(INSTALL_DIR)/$(NAME_BOOT)
	@echo "Installed versioned firmware to $(INSTALL_DIR):"
	@echo "  $(NAME_APP)"
	@echo "  $(NAME_BOOT)"

# release: build + install for both targets in one go. Stages the
# artefacts ready for upload to a GitHub release. Pair with
# RELEASE_VERSION=vX.Y.Z.
#   make release RELEASE_VERSION=v0.2.0
release:
	$(MAKE) TARGET=f103 install-firmware RELEASE_VERSION=$(RELEASE_VERSION)
	$(MAKE) TARGET=f411 install-firmware RELEASE_VERSION=$(RELEASE_VERSION)
	@echo "Release artefacts staged in build/{f103,f411}/install/."
	@echo "Upload to https://github.com/anpeaco/FreeJoyX/releases."

clean:
	$(MAKE) -f makefile.app clean TARGET=$(TARGET)
	$(MAKE) -f makefile.boot clean TARGET=$(TARGET)
	rm -rf build/$(TARGET)/install