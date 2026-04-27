######################################
# target dispatcher
######################################
TARGET ?= f103
include target_$(TARGET).mk

######################################
# product / binary name
######################################
PRODUCT = Bootloader


######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Os


#######################################
# paths
#######################################
# Build path (per-target so f103 / f411 don't collide)
BUILD_DIR = build/$(TARGET)/boot

######################################
# source
######################################
# Board-agnostic bootloader sources. Vendor / driver sources live in
# target_$(TARGET).mk under TARGET_BOOT_C_SOURCES.
BOOT_C_SOURCES =  \
../utils/crc16.c \
../bootloader/Src/main.c \
../bootloader/Src/periphery.c \
../bootloader/Src/stm32f10x_it.c \
../bootloader/Src/usb_desc.c \
../bootloader/Src/usb_endp.c \
../bootloader/Src/usb_hw.c \
../bootloader/Src/usb_istr.c \
../bootloader/Src/usb_prop.c \
../bootloader/Src/usb_pwr.c \

C_SOURCES = $(BOOT_C_SOURCES) $(TARGET_BOOT_C_SOURCES)

# ASM sources come from the target file (different startup per chip family)
ASM_SOURCES = $(TARGET_ASM_SOURCES)


#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
# CPU / FPU / float-abi come from target_$(TARGET).mk
MCU = $(TARGET_CPU) -mthumb $(TARGET_FPU) $(TARGET_FLOAT_ABI)

# AS defines
AS_DEFS =

# C defines from target plus any bootloader-level overrides
C_DEFS = $(TARGET_C_DEFS)

# AS includes
AS_INCLUDES =

# C includes: bootloader's own dir first so its local stm32f10x_conf.h (stripped
# down to just the drivers the bootloader uses) wins over the board's full
# version. Vendor / driver / board dirs come after.
C_INCLUDES = -I../bootloader/Inc \
$(TARGET_C_INCLUDES) \
-I../utils

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"


#######################################
# LDFLAGS
#######################################
# link script (target-specific)
LDSCRIPT = $(TARGET_LDSCRIPT_BOOT)

# libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(PRODUCT).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(PRODUCT).elf $(BUILD_DIR)/$(PRODUCT).bin

#######################################
# build the bootloader
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c makefile.boot target_$(TARGET).mk | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s makefile.boot target_$(TARGET).mk | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(PRODUCT).elf: $(OBJECTS) makefile.boot target_$(TARGET).mk
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir -p $@		

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
  
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
