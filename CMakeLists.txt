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
add_subdirectory(Injector)

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

set(CFG_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR})
set(INSTALL_DIR ${CFG_DIR}/${CMAKE_PROJECT_NAME})
set(CMAKE_INSTALL_PREFIX ${INSTALL_DIR})

install(TARGETS HerosInsightLauncher
    RUNTIME DESTINATION .)

install(TARGETS HerosInsightDLL
    RUNTIME DESTINATION data/
    LIBRARY DESTINATION data/)

add_custom_target(HerosInsight
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${INSTALL_DIR}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${INSTALL_DIR}" --config $<CONFIG>
    DEPENDS HerosInsightLauncher HerosInsightDLL
    COMMENT "Installing mod in ${INSTALL_DIR}"
)

set(DST_ZIP ${INSTALL_DIR}-${PROJECT_VERSION}.zip)
add_custom_target(HerosInsight_Zip
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${INSTALL_DIR}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${INSTALL_DIR}" --config $<CONFIG>
    COMMAND ${CMAKE_COMMAND} -E tar cfv "${DST_ZIP}" --format=zip "${INSTALL_DIR}"
    WORKING_DIRECTORY "${CFG_DIR}"
    DEPENDS HerosInsightLauncher HerosInsightDLL
    COMMENT "Installing and zipping mod to ${DST_ZIP}"
)
