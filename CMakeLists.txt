cmake_minimum_required(VERSION 3.16)

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Read version from VERSION.txt
file(READ "${CMAKE_SOURCE_DIR}/VERSION.txt" PROJECT_VERSION_RAW)
string(STRIP "${PROJECT_VERSION_RAW}" PROJECT_VERSION) # remove whitespace/newlines

project(HerosInsight
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

include_directories(${CMAKE_BINARY_DIR}/generated)

add_subdirectory(Source)
add_subdirectory(Launcher)

configure_file(
    ${CMAKE_SOURCE_DIR}/Source/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

option(PRODUCTION_BUILD "Production build" OFF)
if(PRODUCTION_BUILD)
    target_compile_definitions(HerosInsightLauncher PRIVATE PRODUCTION_BUILD)
    target_compile_definitions(HerosInsightDLL PRIVATE PRODUCTION_BUILD)
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/default_install)

# Set default install components
set(CMAKE_INSTALL_COMPONENT_RUNTIME "runtime")
set(CMAKE_INSTALL_COMPONENT_SYMBOLS "symbols")

install(TARGETS HerosInsightLauncher
    RUNTIME DESTINATION . COMPONENT runtime          # executable goes here
)

install(TARGETS HerosInsightDLL
    RUNTIME DESTINATION data/ COMPONENT runtime      # DLL runtime
    LIBRARY DESTINATION data/ COMPONENT runtime      # shared lib runtime
)

if(MSVC) # Only for MSVC, where PDBs exist
    install(FILES $<TARGET_PDB_FILE:HerosInsightLauncher>
        DESTINATION .
        COMPONENT symbols
        CONFIGURATIONS Debug RelWithDebInfo
        OPTIONAL
    )

    install(FILES $<TARGET_PDB_FILE:HerosInsightDLL>
        DESTINATION .
        COMPONENT symbols
        CONFIGURATIONS Debug RelWithDebInfo
        OPTIONAL
    )
endif()

# set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
# set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
# set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
# set(CPACK_PACKAGE_VENDOR "cfognom")
# set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}")

# include(CPack)
