# target_f411.mk -- STM32F411CEU6 BlackPill V3.x target definitions
#
# Phase 2 minimal blinky: builds a tiny standalone PC13 toggler. No
# application/Src files compile under TARGET=f411 yet -- those carry
# F1-only headers (stm32f10x_*) and references that won't resolve. As
# the BSP grows in Phases 3..7, more LL drivers and shared application
# files will be added to the SRC lists below.
#
# Driver layer is STM32 LL (locked decision in CLAUDE.md / F411 plan).
# HAL is used only for flash (Phase 3) since LL ships no flash driver.

#######################################
# CPU / FPU
#######################################
TARGET_CPU       = -mcpu=cortex-m4
TARGET_FPU       = -mfpu=fpv4-sp-d16
TARGET_FLOAT_ABI = -mfloat-abi=hard

#######################################
# Preprocessor defines
#######################################
TARGET_C_DEFS = \
-DSTM32F411xE \
-DUSE_FULL_LL_DRIVER \
-DUSE_HAL_DRIVER \
-DBOARD_F411_BLACKPILL

# USE_HAL_DRIVER is required because Phase 3 vendors stm32f4xx_hal_flash.c
# (LL doesn't ship a flash driver). The minimal stm32f4xx_hal_conf.h at
# board/f411_blackpill/Inc/stm32f4xx_hal_conf.h enables only HAL_FLASH,
# so the rest of the HAL surface stays out of the F411 image. Adding
# more HAL_*_MODULE_ENABLED entries means breaking the LL-driver-layer
# decision logged in F411_PORT_PLAN.md (2026-04-27).

#######################################
# Vendor / driver / BSP include paths
#######################################
TARGET_C_INCLUDES = \
-I../Drivers/CMSIS/Core/Include \
-I../Drivers/CMSIS/Device/ST/STM32F4xx/Include \
-I../Drivers/STM32F4xx_HAL_Driver/Inc \
-I../board/common/Inc \
-I../board/f411_blackpill/Inc

#######################################
# All C sources for the APPLICATION build (Phase 2: blinky-only).
#######################################
TARGET_APP_C_SOURCES = \
../board/f411_blackpill/Src/main_f411.c \
../board/f411_blackpill/Src/board_init.c \
../board/f411_blackpill/Src/board_flash.c \
../board/f411_blackpill/Src/stm32f4xx_it.c \
../utils/syscalls.c \
../Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_gpio.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rcc.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_utils.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
# stm32f4xx_hal_flash_ramfunc.c deliberately NOT compiled in: it provides
# RAM-resident flash ops for low-power-mode programming, which FreeJoyX
# doesn't use, and it pulls in __HAL_RCC_PWR_CLK_ENABLE which would force
# us to also vendor / enable HAL_RCC. Pure-flash boot-time programming
# uses only stm32f4xx_hal_flash{,_ex}.c.

#######################################
# All C sources for the BOOTLOADER build (Phase 2: stub).
# Phase 6 replaces the stub with a real LL + USBD DFU bootloader.
#######################################
TARGET_BOOT_C_SOURCES = \
../bootloader/f411/Src/main.c \
../utils/syscalls.c \
../Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c

#######################################
# Linker scripts
#######################################
TARGET_LDSCRIPT_APP  = linker_app_f411.ld
TARGET_LDSCRIPT_BOOT = linker_boot_f411.ld

#######################################
# Startup assembly
#######################################
TARGET_ASM_SOURCES = startup_stm32f411xe.s
