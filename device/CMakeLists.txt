cmake_minimum_required(VERSION 3.13)
# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(lighthouse_device)

pico_sdk_init()

set(GCC_MINIMUM_REQUIRED "10")
message(${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION})


if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS GCC_MINIMUM_REQUIRED)
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
endif()

add_subdirectory(src)