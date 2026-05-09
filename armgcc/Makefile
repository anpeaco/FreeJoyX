# Top-level dispatcher. Pick the chip family with TARGET={f103|f411};
# defaults to f103 so a bare `make` keeps working as before.
TARGET ?= f103

# Project semver (FREEJOYX_VERSION) extracted from the synced
# common_defines.h header. Drives the user-facing version on the
# configurator title bar AND the firmware artefact filenames -- one
# answer to "what version is this", regardless of whether someone
# means firmware or configurator. Issue anpeaco/FreeJoyX#18.
#
# FIRMWARE_VERSION (the wire-format hex key) is no longer the source
# for filenames; it stays internal as the &0xFFF0 shape-compat token.
FREEJOYX_VER_MAJOR := $(shell grep -E "^\#define[[:space:]]+FREEJOYX_VERSION_MAJOR" \
                              ../application/Inc/common_defines.h | awk '{print $$3}')
FREEJOYX_VER_MINOR := $(shell grep -E "^\#define[[:space:]]+FREEJOYX_VERSION_MINOR" \
                              ../application/Inc/common_defines.h | awk '{print $$3}')
FREEJOYX_VER_PATCH := $(shell grep -E "^\#define[[:space:]]+FREEJOYX_VERSION_PATCH" \
                              ../application/Inc/common_defines.h | awk '{print $$3}')
FREEJOYX_VERSION := $(FREEJOYX_VER_MAJOR).$(FREEJOYX_VER_MINOR).$(FREEJOYX_VER_PATCH)

# RELEASE_VERSION (optional override). Default = "v<FREEJOYX_VERSION>",
# e.g. "v0.0.0". Set explicitly when packaging a hot-fix release that
# doesn't bump FREEJOYX_VERSION:
#   RELEASE_VERSION=v0.0.0-rc1
#   RELEASE_VERSION=v0.0.0-r1
RELEASE_VERSION ?=
NAME_VERSION := $(if $(RELEASE_VERSION),$(RELEASE_VERSION),v$(FREEJOYX_VERSION))

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