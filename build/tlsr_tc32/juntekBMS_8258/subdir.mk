################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
../../../platform/tc32/div_mod.S 

OBJS += \
./div_mod.o 


# Each subdirectory must supply rules for building sources it contributes
div_mod.o: ../../../platform/tc32/div_mod.S subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 CC/Assembler'
	tc32-elf-gcc -DMCU_CORE_8258 -DMCU_STARTUP_8258 -I../../../apps/juntekBMS -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


