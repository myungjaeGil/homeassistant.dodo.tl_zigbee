################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include zigbee/zdo/subdir.mk
-include zigbee/zcl/zll_commissioning/subdir.mk
-include zigbee/zcl/zcl_wwah/subdir.mk
-include zigbee/zcl/smart_energy/subdir.mk
-include zigbee/zcl/security_safety/subdir.mk
-include zigbee/zcl/ota_upgrading/subdir.mk
-include zigbee/zcl/measument_sensing/subdir.mk
-include zigbee/zcl/light_color_control/subdir.mk
-include zigbee/zcl/hvac/subdir.mk
-include zigbee/zcl/general/subdir.mk
-include zigbee/zcl/commissioning/subdir.mk
-include zigbee/zcl/closures/subdir.mk
-include zigbee/zcl/subdir.mk
-include zigbee/wwah/subdir.mk
-include zigbee/ss/subdir.mk
-include zigbee/ota/subdir.mk
-include zigbee/mac/subdir.mk
-include zigbee/gp/subdir.mk
-include zigbee/common/subdir.mk
-include zigbee/bdb/subdir.mk
-include zigbee/aps/subdir.mk
-include zigbee/af/subdir.mk
-include zbhci/usbPrint/subdir.mk
-include zbhci/usbHID/subdir.mk
-include zbhci/usbCDC/subdir.mk
-include zbhci/uart/subdir.mk
-include zbhci/subdir.mk
-include proj/os/subdir.mk
-include proj/drivers/usb/app/subdir.mk
-include proj/drivers/usb/subdir.mk
-include proj/drivers/subdir.mk
-include proj/common/subdir.mk
-include platform/services/subdir.mk
-include platform/chip_8258/flash/subdir.mk
-include platform/chip_8258/subdir.mk
-include platform/boot/8278/subdir.mk
-include platform/boot/826x/subdir.mk
-include platform/boot/8258/subdir.mk
-include platform/boot/subdir.mk
-include apps/juntekBMS/subdir.mk
-include apps/common/subdir.mk
-include subdir.mk
-include objects.mk

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := juntekBMS_8258
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
LST += \
juntekBMS_8258.lst \

FLASH_IMAGE += \

SIZEDUMMY += \
sizedummy \


# All Target
all:
	+@$(MAKE) --no-print-directory pre-build && $(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: juntekBMS_8258.elf secondary-outputs

# Tool invocations
juntekBMS_8258.elf: $(OBJS) $(USER_OBJS) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: TC32 C Linker'
	tc32-elf-ld --gc-sections -L../../../zigbee/lib/tc32 -L../../../platform/lib -L../../../platform/tc32 -T ../boot.link -o "juntekBMS_8258.elf" $(OBJS) $(USER_OBJS) $(LIBS) -lzb_router -ldrivers_8258 -lsoft-fp
	@echo 'Finished building target: $@'
	@echo ' '

juntekBMS_8258.lst: juntekBMS_8258.elf makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: TC32 Create Extended Listing'
	tc32-elf-objdump -x -D -l -S juntekBMS_8258.elf  >"juntekBMS_8258.lst"
	@echo 'Finished building: $@'
	@echo ' '

: juntekBMS_8258.elf makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Create Flash image (binary format)'
	tc32-elf-objcopy juntekBMS_8258.elf
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: juntekBMS_8258.elf makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: Print Size'
	tc32-elf-size -t juntekBMS_8258.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(FLASH_IMAGE)$(ELFS)$(OBJS)$(LST)$(SIZEDUMMY) juntekBMS_8258.elf
	-@echo ' '

pre-build:
	-"../../../tools/tl_link_load.sh" "../../../platform/boot/8258/boot_8258.link" "D:\workspace\homeassistant.dodo.tl_zigbee\build\tlsr_tc32/boot.link"
	-@echo ' '

post-build:
	-"../../../tools/tl_check_fw.sh" juntekBMS_8258 tc32
	-@echo ' '

secondary-outputs: $(LST) $(FLASH_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents main-build pre-build post-build

-include ../makefile.targets
