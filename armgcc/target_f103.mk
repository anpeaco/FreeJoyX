# target_f103.mk -- STM32F103 BluePill target definitions
#
# All F103-specific compiler flags, full C/ASM source list, and
# linker / startup file names live here so that makefile.app and
# makefile.boot can stay target-agnostic. Phase 2 of the F411 port will
# add a parallel target_f411.mk with the equivalent F4 / LL configuration.

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
# All C sources for the APPLICATION build.
#
# This list is the full source set -- application logic, BSP, vendor
# drivers, USB stack -- because makefile.app no longer carries any
# hardcoded source list. Each target's makefile defines what it builds.
#######################################
TARGET_APP_C_SOURCES = \
../application/Src/analog.c \
../application/Src/axis_to_buttons.c \
../application/Src/buttons.c \
../application/Src/encoders.c \
../application/Src/image_id.c \
../application/Src/sensor_dispatch.c \
../application/Src/config.c \
../application/Src/bitmap.c \
../application/Src/led_effects.c \
../application/Src/simhub.c \
../application/Src/leds.c \
../application/Src/main.c \
../application/Src/usb_app.c \
../application/Src/joy_report_desc.c \
../application/Src/periphery.c \
../application/Src/tle5011.c \
../application/Src/tle5012.c \
../application/Src/as5600.c \
../application/Src/as5048a.c \
../application/Src/ads1115.c \
../application/Src/mlx90363.c \
../application/Src/mlx90393.c \
../application/Src/mcp320x.c \
../application/Src/shift_registers.c \
../application/Src/stm32f10x_it.c \
../application/Src/usb_desc.c \
../application/Src/usb_endp.c \
../application/Src/usb_hw.c \
../application/Src/usb_istr.c \
../application/Src/usb_prop.c \
../application/Src/usb_pwr.c \
../utils/crc16.c \
../utils/syscalls.c \
../board/f103_bluepill/Src/board_pins.c \
../board/f103_bluepill/Src/board_flash.c \
../board/f103_bluepill/Src/board_dfu.c \
../board/f103_bluepill/Src/board_tick.c \
../board/f103_bluepill/Src/board_encoder.c \
../board/f103_bluepill/Src/board_spi.c \
../board/f103_bluepill/Src/board_i2c.c \
../board/f103_bluepill/Src/board_uart.c \
../board/f103_bluepill/Src/board_usb.c \
../board/f103_bluepill/Src/board_misc.c \
../board/f103_bluepill/Src/board_ws2812b.c \
../board/f103_bluepill/Src/board_pwm.c \
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
# All C sources for the BOOTLOADER build.
#######################################
TARGET_BOOT_C_SOURCES = \
../utils/crc16.c \
../utils/syscalls.c \
../bootloader/Src/main.c \
../bootloader/Src/periphery.c \
../bootloader/Src/stm32f10x_it.c \
../bootloader/Src/usb_desc.c \
../bootloader/Src/usb_endp.c \
../bootloader/Src/usb_hw.c \
../bootloader/Src/usb_istr.c \
../bootloader/Src/usb_prop.c \
../bootloader/Src/usb_pwr.c \
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
