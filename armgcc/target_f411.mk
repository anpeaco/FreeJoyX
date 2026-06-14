# target_f411.mk -- STM32F411CEU6 BlackPill V3.x target definitions
#
# Builds the full FreeJoyX application and the DFU bootloader for the F411:
# the board-agnostic application/Src files plus the F411 BSP (board/
# f411_blackpill/Src), the vendored STM32 LL drivers, the HAL flash + PCD
# drivers, and the Cube USB Device library. The SRC lists below are the
# authoritative set of translation units for each image.
#
# Driver layer is STM32 LL (locked decision in CLAUDE.md / F411 plan).
# HAL is used only for flash and the USB PCD since LL ships neither.

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
-I../Drivers/STM32_USB_Device_Library/Core/Inc \
-I../Drivers/STM32_USB_Device_Library/Class/CustomHID/Inc \
-I../board/common/Inc \
-I../board/f411_blackpill/Inc

#######################################
# C sources for the APPLICATION build: the shared application/Src files,
# the F411 BSP, the LL + HAL(flash/PCD) drivers, and the Cube USB Device
# library. Entry point is application/Src/main.c (shared with F103).
#######################################
TARGET_APP_C_SOURCES = \
../application/Src/main.c \
../board/f411_blackpill/Src/board_init.c \
../board/f411_blackpill/Src/board_flash.c \
../board/f411_blackpill/Src/board_tick.c \
../board/f411_blackpill/Src/board_encoder.c \
../board/f411_blackpill/Src/board_pins.c \
../board/f411_blackpill/Src/board_spi.c \
../board/f411_blackpill/Src/board_i2c.c \
../board/f411_blackpill/Src/board_uart.c \
../board/f411_blackpill/Src/board_dfu.c \
../board/f411_blackpill/Src/board_misc.c \
../board/f411_blackpill/Src/board_pwm.c \
../board/f411_blackpill/Src/board_ws2812b.c \
../board/f411_blackpill/Src/board_phase_stubs.c \
../board/f411_blackpill/Src/usbd_freejoy_desc.c \
../board/f411_blackpill/Src/usbd_freejoy_if.c \
../board/f411_blackpill/Src/usbd_freejoy_class.c \
../board/f411_blackpill/Src/usbd_conf.c \
../board/f411_blackpill/Src/board_usb.c \
../board/f411_blackpill/Src/stm32f4xx_it.c \
../board/f411_blackpill/Src/board_sensor_irqs.c \
../application/Src/buttons.c \
../application/Src/encoders.c \
../application/Src/image_id.c \
../application/Src/analog.c \
../application/Src/sensor_dispatch.c \
../application/Src/usb_app.c \
../application/Src/joy_report_desc.c \
../application/Src/periphery.c \
../application/Src/axis_to_buttons.c \
../application/Src/bitmap.c \
../application/Src/simhub.c \
../application/Src/config.c \
../application/Src/shift_registers.c \
../application/Src/leds.c \
../application/Src/led_effects.c \
../application/Src/tle5011.c \
../application/Src/tle5012.c \
../application/Src/as5048a.c \
../application/Src/mcp320x.c \
../application/Src/mlx90363.c \
../application/Src/mlx90393.c \
../application/Src/as5600.c \
../application/Src/ads1115.c \
../utils/crc16.c \
../utils/syscalls.c \
../Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_dma.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_gpio.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_pwr.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rcc.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_spi.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_tim.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_utils.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_core.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
../Drivers/STM32_USB_Device_Library/Class/CustomHID/Src/usbd_customhid.c
# stm32f4xx_hal_flash_ramfunc.c deliberately NOT compiled in: it provides
# RAM-resident flash ops for low-power-mode programming, which FreeJoyX
# doesn't use, and it pulls in __HAL_RCC_PWR_CLK_ENABLE which would force
# us to also vendor / enable HAL_RCC. Pure-flash boot-time programming
# uses only stm32f4xx_hal_flash{,_ex}.c.

#######################################
# All C sources for the BOOTLOADER build: the LL + USBD DFU bootloader
# (bootloader/f411/Src) plus the shared F411 USB descriptor/config + clock
# init and the LL/HAL/USBD drivers it needs.
#######################################
TARGET_BOOT_C_SOURCES = \
../bootloader/f411/Src/main.c \
../bootloader/f411/Src/boot_usb_if.c \
../bootloader/f411/Src/stm32f4xx_it.c \
../board/f411_blackpill/Src/usbd_conf.c \
../board/f411_blackpill/Src/usbd_freejoy_desc.c \
../board/f411_blackpill/Src/board_init.c \
../utils/crc16.c \
../utils/syscalls.c \
../Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_gpio.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_pwr.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rcc.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c \
../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_utils.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_core.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
../Drivers/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
../Drivers/STM32_USB_Device_Library/Class/CustomHID/Src/usbd_customhid.c

#######################################
# Linker scripts
#######################################
TARGET_LDSCRIPT_APP  = linker_app_f411.ld
TARGET_LDSCRIPT_BOOT = linker_boot_f411.ld

#######################################
# Startup assembly
#######################################
TARGET_ASM_SOURCES = startup_stm32f411xe.s
