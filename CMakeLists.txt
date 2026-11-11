
cmake_minimum_required(VERSION 3.22)

#
# STM32F411 Speed Signal Translator
# USB CDC + PWM Control
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Speed_Signal_Translator)

# Enable compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable C and ASM
enable_language(C ASM)

# Create executable
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user library search paths here
)

# Add project sources
target_sources(${CMAKE_PROJECT_NAME} PRIVATE

    # PWM frequency and duty cycle control
    Src/pwm_control.c

)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE

    # Project header files
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc

)

# Add project symbols
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user-defined symbols here
)

# Remove incorrect library dependency
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Link STM32 libraries
target_link_libraries(${CMAKE_PROJECT_NAME}

    stm32cubemx

)
