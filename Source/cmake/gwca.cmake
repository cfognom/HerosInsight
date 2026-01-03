include_guard()

# Define the path to the local cached copy
set(GWCA_LOCAL_PATH "${CMAKE_SOURCE_DIR}/Source/GWCA_BACKUP/gwca-src")

# # Attempt to fetch the repository
# include(FetchContent)
# FetchContent_Declare(
#     gwca
#     GIT_REPOSITORY https://github.com/gwdevhub/gwca
#     GIT_TAG master
# )
# FetchContent_GetProperties(gwca)
# FetchContent_Populate(gwca)

# Use the local cached copy if it exists
message(STATUS "Using local cached copy of GWCA.")
set(gwca_SOURCE_DIR ${GWCA_LOCAL_PATH})
    
add_subdirectory(${gwca_SOURCE_DIR} EXCLUDE_FROM_ALL)