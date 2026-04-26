# target_f103.mk -- STM32F103 BluePill target definitions
#
# All F103-specific compiler flags, vendor / driver source lists, and
# linker / startup file names live here so that makefile.app and
# makefile.boot can stay target-agnostic. Phase 2 of the F411 port will
# add a parallel target_f411.mk with the equivalent F4 / HAL configuration.

#######################################
# CPU / FPU
#######################################
TARGET_CPU       = -mcpu=cortex-m3
TARGET_FPU       =
TARGET_FLOAT_ABI =

#######################################
# Preprocessor defines
#######################################
TARGET_C_DEFS = \
-DUSE_STDPERIPH_DRIVER \
-DSTM32F10X_MD \
-DBOARD_F103_BLUEPILL

#######################################
# Vendor / driver / BSP include paths
#######################################
TARGET_C_INCLUDES = \
-I../Drivers/CMSIS/CM3/CoreSupport \
-I../Drivers/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
-I../Drivers/STM32F10x_StdPeriph_Driver/inc \
-I../Drivers/STM32_USB-FS-Device_Driver/inc \
-I../board/common/Inc \
-I../board/f103_bluepill/Inc

#######################################
# Vendor / driver C sources used by the APPLICATION
#
# Board BSP sources (board/f103_bluepill/Src/*.c) are co-located with the
# vendor drivers below since they're equally chip-specific.
#######################################
TARGET_APP_C_SOURCES = \
../board/f103_bluepill/Src/board_pins.c \
../Drivers/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/misc.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_adc.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_flash.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_core.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_init.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_int.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_mem.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_regs.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_sil.c

#######################################
# Vendor / driver C sources used by the BOOTLOADER
#######################################
TARGET_BOOT_C_SOURCES = \
../Drivers/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c \
../Drivers/CMSIS/CM3/CoreSupport/core_cm3.c \
../Drivers/STM32F10x_StdPeriph_Driver/src/misc.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_core.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_init.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_int.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_mem.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_regs.c \
../Drivers/STM32_USB-FS-Device_Driver/src/usb_sil.c

#######################################
# Linker scripts
#######################################
TARGET_LDSCRIPT_APP  = linker_app.ld
TARGET_LDSCRIPT_BOOT = linker_boot.ld

#######################################
# Startup assembly
#######################################
TARGET_ASM_SOURCES = startup_stm32f103xb.s
