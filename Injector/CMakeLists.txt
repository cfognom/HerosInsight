add_executable(Injector main.cpp)

set_target_properties(Injector PROPERTIES
    WIN32_EXECUTABLE TRUE # Set the WIN32 flag to create a Windows application
    OUTPUT_NAME "Launch_HerosInsight"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/package"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/pdb"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

find_package(curl CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
target_link_libraries(Injector PRIVATE
    CURL::libcurl
    libzip::zip
)