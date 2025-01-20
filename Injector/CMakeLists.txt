add_executable(Injector main.cpp)

# Set the WIN32 flag to create a Windows application
set_target_properties(Injector PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(Injector PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..)
set_target_properties(Injector PROPERTIES OUTPUT_NAME "Launch_HerosInsight")