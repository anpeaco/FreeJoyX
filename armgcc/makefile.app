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
# All sources -- application logic, BSP, vendor drivers, USB stack --
# come from target_$(TARGET).mk so each chip target can declare its own
# minimal SRC list (F411 doesnt compile F1-only files like stm32f10x_it.c).
C_SOURCES = $(TARGET_APP_C_SOURCES)

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
# -specs=nosys.specs: provide newlib syscall stubs (_close/_lseek/_read/_write etc.)
#   so the link-time GC's "not implemented and will always fail" warnings from
#   libc_nano go quiet. Pairs with -lnosys above; nano.specs stays first.
# -Wl,--no-warn-rwx-segments: Cortex-M canonical LOAD segment holds .text and
#   the .data initialiser image in flash, which trips the RWX-segment warning
#   from ld 2.39+. The arrangement is intentional and safe on this target.
LDFLAGS = $(MCU) -specs=nano.specs -specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(PRODUCT).map,--cref -Wl,--gc-sections -Wl,--no-warn-rwx-segments

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
