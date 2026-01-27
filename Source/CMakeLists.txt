list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

add_library(HerosInsight SHARED
    main.cpp
    constants.h
    imgui_impl_dx9.cpp
    imgui_impl_win32.cpp
    imgui_customize.cpp
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
    encstr_debugger.cpp
    custom_agent_data.h
    custom_agent_data.cpp
    attribute_or_title.cpp
    effect_initiator.cpp
    StoC_packets.h
    autovec.h
    packet_stepper.cpp
    matcher.cpp
    text_provider.cpp
    filtering.cpp
    rich_text.cpp
    capacity_hints.cpp
    string_manager.cpp
    settings.cpp
)

# Headers live here
target_include_directories(HerosInsight PRIVATE
    .
)

# Precompiled header
target_precompile_headers(HerosInsight PRIVATE pch.h)

# Dependencies
include(gwca)
include(imgui)
# include(simdutf)

target_link_libraries(HerosInsight PRIVATE
    gwca
    imgui
    # simdutf
)

# Add custom command to copy the entire resources folder to the output directory
add_custom_command(
    TARGET HerosInsight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:HerosInsight>/resources
    COMMENT "Copying resources folder to output directory"
)

# Source/CMakeLists.txt
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/package/cache/bin")

set_target_properties(HerosInsight PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}" # .dll goes here
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}" # .so / .dll cross-platform
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}" # .lib
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/pdb"
)