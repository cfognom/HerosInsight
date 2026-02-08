add_executable(HerosInsightLauncher main.cpp app_icon.rc)

set_target_properties(HerosInsightLauncher PROPERTIES
    WIN32_EXECUTABLE TRUE # Set the WIN32 flag to create a Windows application
    OUTPUT_NAME "Launch_HerosInsight"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

find_package(curl CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
target_link_libraries(HerosInsightLauncher PRIVATE
    CURL::libcurl
    libzip::zip
)