################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/engines/s_arp.c \
../Src/engines/s_arp_chord.c \
../Src/engines/s_chord.c 

OBJS += \
./Src/engines/s_arp.o \
./Src/engines/s_arp_chord.o \
./Src/engines/s_chord.o 

C_DEPS += \
./Src/engines/s_arp.d \
./Src/engines/s_arp_chord.d \
./Src/engines/s_chord.d 


# Each subdirectory must supply rules for building sources it contributes
Src/engines/s_arp.o: ../Src/engines/s_arp.c Src/engines/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F0 -DSTM32F030C8Tx -DSTM32F030x8 -c -I../Inc -I../CMSIS/Device/ST/STM32F0xx/Include -I../CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/engines/s_arp.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/engines/s_arp_chord.o: ../Src/engines/s_arp_chord.c Src/engines/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F0 -DSTM32F030C8Tx -DSTM32F030x8 -c -I../Inc -I../CMSIS/Device/ST/STM32F0xx/Include -I../CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/engines/s_arp_chord.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/engines/s_chord.o: ../Src/engines/s_chord.c Src/engines/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F0 -DSTM32F030C8Tx -DSTM32F030x8 -c -I../Inc -I../CMSIS/Device/ST/STM32F0xx/Include -I../CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/engines/s_chord.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

