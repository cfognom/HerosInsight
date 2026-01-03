cmake_minimum_required(VERSION 3.16)

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

project(HerosInsight
    VERSION 0.1.0
    LANGUAGES CXX
)

include_directories(${CMAKE_BINARY_DIR}/generated)

add_subdirectory(Source)
add_subdirectory(Injector)

configure_file(
    ${CMAKE_SOURCE_DIR}/Source/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)
