# Top-level dispatcher. Pick the chip family with TARGET={f103|f411};
# defaults to f103 so a bare `make` keeps working as before.
TARGET ?= f103

# Wire-format version extracted from the firmware's common_defines.h
# header. Used to name versioned firmware artefacts so multiple builds
# can coexist in the configurator's firmware/ folder without colliding.
# Format example: 0x1770 -> "1770".
FW_VERSION := $(shell grep -E "^\#define FIRMWARE_VERSION" \
                       ../application/Inc/common_defines.h | \
                  awk '{print $$3}' | sed 's/0x//' | head -c 4)

# RELEASE_VERSION (optional): when publishing a tagged release, set this
# to the release tag (e.g. v0.2.0) so the versioned artefacts are named
# after the human-friendly tag rather than the raw firmware-version mask.
# Convention:
#   RELEASE_VERSION unset    -> freejoyx-f103-app-fw1770.bin   (dev build)
#   RELEASE_VERSION=v0.2.0   -> freejoyx-f103-app-v0.2.0.bin   (tagged release)
RELEASE_VERSION ?=
NAME_VERSION := $(if $(RELEASE_VERSION),$(RELEASE_VERSION),fw$(FW_VERSION))

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