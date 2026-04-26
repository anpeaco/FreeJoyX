######################################
# target dispatcher
######################################
TARGET ?= f103
include target_$(TARGET).mk

######################################
# product / binary name
######################################
PRODUCT = FreeJoy

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization. Note: with -O3 the binary is not fitted in 64k anymore
OPT = -O2


#######################################
# paths
#######################################
# Build path (per-target so f103 / f411 don't collide)
BUILD_DIR = build/$(TARGET)/app

######################################
# source
######################################
# Board-agnostic application sources. Vendor / driver sources live in
# target_$(TARGET).mk under TARGET_APP_C_SOURCES.
APP_C_SOURCES =  \
../application/Src/analog.c \
../application/Src/axis_to_buttons.c \
../application/Src/buttons.c \
../utils/crc16.c \
../application/Src/encoders.c \
../application/Src/config.c \
../application/Src/bitmap.c \
../application/Src/led_effects.c \
../application/Src/simhub.c \
../application/Src/leds.c \
../application/Src/main.c \
../application/Src/periphery.c \
../application/Src/tle5011.c \
../application/Src/tle5012.c \
../application/Src/as5600.c \
../application/Src/as5048a.c \
../application/Src/ads1115.c \
../application/Src/mlx90363.c \
../application/Src/mlx90393.c \
../application/Src/mcp320x.c \
../application/Src/ws2812b.c \
../application/Src/shift_registers.c \
../application/Src/spi.c \
../application/Src/i2c.c \
../application/Src/uart.c \
../application/Src/stm32f10x_it.c \
../application/Src/usb_desc.c \
../application/Src/usb_endp.c \
../application/Src/usb_hw.c \
../application/Src/usb_istr.c \
../application/Src/usb_prop.c \
../application/Src/usb_pwr.c \

C_SOURCES = $(APP_C_SOURCES) $(TARGET_APP_C_SOURCES)

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

# C defines from target plus any application-level overrides
C_DEFS = $(TARGET_C_DEFS)

# AS includes
AS_INCLUDES = 

# C includes: vendor / driver dirs from target, plus the application's own
C_INCLUDES = $(TARGET_C_INCLUDES) \
-I../application/Inc \
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
LDSCRIPT = $(TARGET_LDSCRIPT_APP)

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(PRODUCT).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(PRODUCT).elf $(BUILD_DIR)/$(PRODUCT).bin $(BUILD_DIR)/$(PRODUCT).hex 

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c makefile.app target_$(TARGET).mk | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s makefile.app target_$(TARGET).mk | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(PRODUCT).elf: $(OBJECTS) makefile.app target_$(TARGET).mk
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
