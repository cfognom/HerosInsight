cmake_minimum_required(VERSION 3.16)

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(HerosInsight)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

add_library(HerosInsight SHARED
    main.cpp
    constants.h
    imgui_impl_dx9.cpp
    utils.cpp
    hero_ai.cpp
    party_data.cpp
    update_manager.cpp
    packet_reader.cpp
    behaviours.cpp
    behaviours.h
    debug_display.cpp
    energy_display.cpp
    damage_display.cpp
    effect_tracking.cpp
    effect_tracking.h
    debug.cpp
    debug_ui.cpp
    texture_module.cpp
    worldspaceUI.cpp
    custom_skill_data.cpp
    skill_book.cpp
    texture_viewer.cpp
    custom_agent_data.h
    custom_agent_data.cpp
    scanner_tool.cpp
    attribute_or_title.cpp
    attribute_store.cpp
    effect_initiator.cpp
    StoC_packets.h
    autovec.h
    packet_stepper.cpp
    matcher.cpp
    skill_text_provider.cpp
    )

include_directories(HerosInsight PRIVATE ${CMAKE_SOURCE_DIR})
target_precompile_headers(HerosInsight PRIVATE pch.h)

include(gwca)
include(imgui)
target_link_libraries(HerosInsight PRIVATE
    gwca
    imgui
)

add_subdirectory(Injector)

# Add custom command to copy the entire resources folder to the output directory
add_custom_command(
    TARGET HerosInsight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:HerosInsight>/resources
    COMMENT "Copying resources folder to output directory"
)