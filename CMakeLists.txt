cmake_minimum_required(VERSION 3.20)
project(nba_storage C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

include_directories(include)

add_executable(nba_storage
    src/main.c
    src/b_plus_tree.c
    src/block.c
    src/nba_record.c
    src/statistics.c
    src/storage.c
    src/utils.c 
    src/buffer_manager.c
)

# Force executable to be in project root regardless of config
set_target_properties(nba_storage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
)
