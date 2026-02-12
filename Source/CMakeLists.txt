list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")
add_library(HerosInsightDLL SHARED ${SOURCES})

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