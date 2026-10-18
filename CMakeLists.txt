cmake_minimum_required(VERSION 3.25)
project(ChessWhy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CHESSWHY_ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets" CACHE PATH "Engine and data assets (tools/fetch_assets.sh)")
set(CHESSWHY_PYTHON "${PROJECT_SOURCE_DIR}/.venv/bin/python" CACHE FILEPATH "Python with tools/requirements-dev.txt installed")
option(CHESSWHY_CHECK_TIMING "Assert the standard-budget wall-time limit in engine tests" ON)

# ---- Third-party (pinned submodules, see third_party/DEPENDENCIES.md) ----
add_library(chess_library INTERFACE)
target_include_directories(chess_library SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/third_party/chess-library/include")

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/json EXCLUDE_FROM_ALL SYSTEM)
add_subdirectory(third_party/Catch2 EXCLUDE_FROM_ALL SYSTEM)

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/third_party/Catch2/extras")
include(Catch)

# ---- Shared settings for ChessWhy targets ----
add_library(chesswhy_headers INTERFACE)
target_include_directories(chesswhy_headers INTERFACE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(chesswhy_headers INTERFACE chess_library nlohmann_json::nlohmann_json)

add_library(chesswhy_warnings INTERFACE)
target_compile_options(chesswhy_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)

# Paths for the CLI defaults and for tests (fixtures, schema).
add_library(chesswhy_paths INTERFACE)
target_compile_definitions(chesswhy_paths INTERFACE
    CHESSWHY_ASSETS_DIR="${CHESSWHY_ASSETS_DIR}"
    CHESSWHY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# ---- Modules (Planning/milestones/M1.md, Build layout) ----
foreach(module IN ITEMS input engine openings report render analysis cli pgntool)
    add_subdirectory("src/${module}")
    add_subdirectory("tests/${module}")
endforeach()
