include(FetchContent)
FetchContent_Declare(pico_sdk URL https://github.com/raspberrypi/pico-sdk/archive/refs/tags/1.3.1.tar.gz)
FetchContent_MakeAvailable(pico_sdk)

project(lighthouse_device)

pico_sdk_init()

cmake_minimum_required(VERSION 3.10)
set(GCC_MINIMUM_REQUIRED "10")
message(${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION})


if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS GCC_MINIMUM_REQUIRED)
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
endif()

add_subdirectory(src)