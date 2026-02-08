list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

add_library(HerosInsightDLL SHARED
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
    autovec.h
    packet_stepper.cpp
    matcher.cpp
    text_provider.cpp
    filtering.cpp
    rich_text.cpp
    capacity_hints.cpp
    string_manager.cpp
    settings.cpp
    imgui_ext.h
)

set_target_properties(HerosInsightDLL PROPERTIES
    OUTPUT_NAME "HerosInsight"
)

# Headers live here
target_include_directories(HerosInsightDLL PRIVATE
    .
)

# Precompiled header
target_precompile_headers(HerosInsightDLL PRIVATE pch.h)

# Dependencies
include(gwca)
include(imgui)
# include(simdutf)

target_link_libraries(HerosInsightDLL PRIVATE
    gwca
    imgui
    # simdutf
)

# # Add custom command to copy the entire resources folder to the output directory
# add_custom_command(
#     TARGET HerosInsightDLL POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#             ${PROJECT_SOURCE_DIR}/resources
#             $<TARGET_FILE_DIR:HerosInsightDLL>/resources
#     COMMENT "Copying resources folder to output directory"
# )

# Source/CMakeLists.txt