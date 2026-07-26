################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"F:/TI/ccstheia/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64" -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64/Debug" -I"F:/TI/mspm0_sdk_2_10_00_04/source/third_party/CMSIS/Core/Include" -I"F:/TI/mspm0_sdk_2_10_00_04/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1849511292: ../empty.syscfg
	@echo 'SysConfig - building file: "$<"'
	"F:/TI/ccstheia/ccs/utils/sysconfig_1.27.1/sysconfig_cli.bat" -s "F:/TI/mspm0_sdk_2_10_00_04/.metadata/product.json" --script "C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64/empty.syscfg" -o "." --compiler ticlang
	@echo 'Finished building: "$<"'
	@echo ' '

device_linker.cmd: build-1849511292 ../empty.syscfg
device.opt: build-1849511292
device.cmd.genlibs: build-1849511292
ti_msp_dl_config.c: build-1849511292
ti_msp_dl_config.h: build-1849511292
Event.dot: build-1849511292

%.o: ./%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"F:/TI/ccstheia/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64" -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64/Debug" -I"F:/TI/mspm0_sdk_2_10_00_04/source/third_party/CMSIS/Core/Include" -I"F:/TI/mspm0_sdk_2_10_00_04/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

startup_mspm0g350x_ticlang.o: F:/TI/mspm0_sdk_2_10_00_04/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"F:/TI/ccstheia/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64" -I"C:/Users/Thorn/workspace_ccstheia/TI_Car_3507_64/Debug" -I"F:/TI/mspm0_sdk_2_10_00_04/source/third_party/CMSIS/Core/Include" -I"F:/TI/mspm0_sdk_2_10_00_04/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


