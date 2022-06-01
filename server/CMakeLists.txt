project(lighthouse)


cmake_minimum_required(VERSION 3.10)
set(GCC_MINIMUM_REQUIRED "11")
message(${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION})


if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS GCC_MINIMUM_REQUIRED)
        message(FATAL_ERROR "Project (${PROJECT_NAME}) requires GCC version of at least ${GCC_MINIMUM_REQUIRED}")
endif()

include(FetchContent)

add_subdirectory(src)