
cmake_minimum_required(VERSION 3.22)

#
# STM32F411 Speed Signal Translator
# UTRA Embedded Team
#
# Features:
# - USB CDC communication
# - TIM1 PWM generation
# - Sine-shaped PWM duty cycle
# - TIM2 Input Capture
#

# C language settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Project name
set(CMAKE_PROJECT_NAME Speed_Signal_Translator)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Initialize project
project(${CMAKE_PROJECT_NAME})

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Enable C and Assembly
enable_language(C ASM)

# Create firmware executable
add_executable(${CMAKE_PROJECT_NAME})

# STM32CubeMX generated code
add_subdirectory(cmake/stm32cubemx)

# Additional linker directories
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add custom library directories here
)

# Application source files
target_sources(${CMAKE_PROJECT_NAME} PRIVATE

    # Adjustable PWM via USB CDC
    Src/pwm_control.c

    # Automatic sine-shaped PWM
    Src/sine_pwm.c

)

# Application include directories
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/Inc

)

# Compiler definitions
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add application-specific definitions here
)

# Remove incorrect implicit library dependency
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Link STM32CubeMX and math library
target_link_libraries(${CMAKE_PROJECT_NAME}

    stm32cubemx

    # Required for cosf()
    m

)

