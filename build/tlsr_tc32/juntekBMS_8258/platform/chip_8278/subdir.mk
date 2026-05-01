################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../../../platform/chip_8278/flash.c 

OBJS += \
./platform/chip_8278/flash.o 


# Each subdirectory must supply rules for building sources it contributes
platform/chip_8278/flash.o: ../../../platform/chip_8278/flash.c platform/chip_8278/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/juntekBMS -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zigbee/wwah -I../../../zbhci -DROUTER=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_DIMMABLE_LIGHT__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


