################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/UNIX-Serial-2-CSV.c 

C_DEPS += \
./src/UNIX-Serial-2-CSV.d 

OBJS += \
./src/UNIX-Serial-2-CSV.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/UNIX-Serial-2-CSV.d ./src/UNIX-Serial-2-CSV.o

.PHONY: clean-src

