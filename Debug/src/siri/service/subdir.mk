################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/service/account.c \
../src/siri/service/client.c \
../src/siri/service/request.c

OBJS += \
./src/siri/service/account.o \
./src/siri/service/client.o \
./src/siri/service/request.o

C_DEPS += \
./src/siri/service/account.d \
./src/siri/service/client.d \
./src/siri/service/request.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/service/%.o: ../src/siri/service/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O0 -g3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


