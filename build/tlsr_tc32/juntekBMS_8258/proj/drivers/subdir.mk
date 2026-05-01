################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../../../proj/drivers/drv_adc.c \
../../../proj/drivers/drv_calibration.c \
../../../proj/drivers/drv_console.c \
../../../proj/drivers/drv_flash.c \
../../../proj/drivers/drv_gpio.c \
../../../proj/drivers/drv_hw.c \
../../../proj/drivers/drv_i2c.c \
../../../proj/drivers/drv_keyboard.c \
../../../proj/drivers/drv_nv.c \
../../../proj/drivers/drv_pm.c \
../../../proj/drivers/drv_putchar.c \
../../../proj/drivers/drv_pwm.c \
../../../proj/drivers/drv_security.c \
../../../proj/drivers/drv_spi.c \
../../../proj/drivers/drv_timer.c \
../../../proj/drivers/drv_uart.c 

OBJS += \
./proj/drivers/drv_adc.o \
./proj/drivers/drv_calibration.o \
./proj/drivers/drv_console.o \
./proj/drivers/drv_flash.o \
./proj/drivers/drv_gpio.o \
./proj/drivers/drv_hw.o \
./proj/drivers/drv_i2c.o \
./proj/drivers/drv_keyboard.o \
./proj/drivers/drv_nv.o \
./proj/drivers/drv_pm.o \
./proj/drivers/drv_putchar.o \
./proj/drivers/drv_pwm.o \
./proj/drivers/drv_security.o \
./proj/drivers/drv_spi.o \
./proj/drivers/drv_timer.o \
./proj/drivers/drv_uart.o 


# Each subdirectory must supply rules for building sources it contributes
proj/drivers/drv_adc.o: ../../../proj/drivers/drv_adc.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_calibration.o: ../../../proj/drivers/drv_calibration.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_console.o: ../../../proj/drivers/drv_console.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_flash.o: ../../../proj/drivers/drv_flash.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_gpio.o: ../../../proj/drivers/drv_gpio.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_hw.o: ../../../proj/drivers/drv_hw.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_i2c.o: ../../../proj/drivers/drv_i2c.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_keyboard.o: ../../../proj/drivers/drv_keyboard.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_nv.o: ../../../proj/drivers/drv_nv.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_pm.o: ../../../proj/drivers/drv_pm.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_putchar.o: ../../../proj/drivers/drv_putchar.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_pwm.o: ../../../proj/drivers/drv_pwm.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_security.o: ../../../proj/drivers/drv_security.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_spi.o: ../../../proj/drivers/drv_spi.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_timer.o: ../../../proj/drivers/drv_timer.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/drivers/drv_uart.o: ../../../proj/drivers/drv_uart.c proj/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


