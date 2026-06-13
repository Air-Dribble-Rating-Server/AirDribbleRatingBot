cmake_minimum_required(VERSION 3.20)
project(AirDribbleRatingBot VERSION 1.0.0 LANGUAGES CXX)

# ─── C++ Standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── MSVC‑specific configuration ───────────────────────────────
if(MSVC)
    add_compile_options(
        "$<$<CONFIG:Debug>:/Od;/Zi;/W4>"
        "$<$<CONFIG:Release>:/O2;/Ob2;/Oi;/Ot;/Oy;/GL;/DNDEBUG;/W0>"
    )
    add_compile_definitions(
        "$<$<CONFIG:Release>:NDEBUG>"
    )
    # Linking
    add_link_options(
        "$<$<CONFIG:Release>:/LTCG;/OPT:REF;/OPT:ICF>"
    )
    # Debug flags for the linker
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    # Use static CRT in both configurations (matches D++ precompiled library)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# ─── Dependencies ──────────────────────────────────────────────
include(FetchContent)

# D++ 10.1.5 precompiled for Windows (MSVC)
set(DPP_VERSION "10.1.5")
set(DPP_URL "https://github.com/brainboxdotcc/DPP/releases/download/v${DPP_VERSION}/libdpp-${DPP_VERSION}-win64-release-vs2022.zip")
set(DPP_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/dpp-archive")
set(DPP_EXTRACT_DIR "${CMAKE_BINARY_DIR}/_deps/dpp-extracted")
set(DPP_ROOT "")

# 1. Try to find existing extraction
file(GLOB_RECURSE DPP_LIB_FOUND "${DPP_EXTRACT_DIR}/**/dpp.lib")

if(DPP_LIB_FOUND)
    get_filename_component(DPP_LIB_DIR "${DPP_LIB_FOUND}" DIRECTORY)
    get_filename_component(DPP_LIB_PARENT "${DPP_LIB_DIR}" DIRECTORY)
    get_filename_component(DPP_ROOT "${DPP_LIB_PARENT}" DIRECTORY)
    message(STATUS "Found existing D++ at: ${DPP_ROOT}")
else()
    if(NOT EXISTS "${DPP_DOWNLOAD_DIR}/dpp.zip")
        file(DOWNLOAD "${DPP_URL}" "${DPP_DOWNLOAD_DIR}/dpp.zip" SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download D++: ${DOWNLOAD_STATUS}")
        endif()
    endif()
    file(REMOVE_RECURSE "${DPP_EXTRACT_DIR}")
    file(ARCHIVE_EXTRACT
        INPUT "${DPP_DOWNLOAD_DIR}/dpp.zip"
        DESTINATION "${DPP_EXTRACT_DIR}"
    )
    file(GLOB DPP_INNER_DIRS LIST_DIRECTORIES true "${DPP_EXTRACT_DIR}/*")
    list(LENGTH DPP_INNER_DIRS NUM_DIRS)
    if(NUM_DIRS EQUAL 0)
        message(FATAL_ERROR "D++ extraction failed: no directory found")
    endif()
    list(GET DPP_INNER_DIRS 0 DPP_ROOT)
    message(STATUS "Extracted D++ to: ${DPP_ROOT}")
endif()

# 2. Verify headers (includes nlohmann/json.hpp inside dpp/nlohmann)
set(DPP_INCLUDE_DIR "${DPP_ROOT}/include/dpp-10.1")
set(DPP_EXTRA_INCLUDE_DIR "${DPP_INCLUDE_DIR}/dpp")  # for nlohmann/json.hpp
if(NOT EXISTS "${DPP_INCLUDE_DIR}/dpp/dpp.h")
    message(FATAL_ERROR "D++ headers not found at ${DPP_INCLUDE_DIR}/dpp/dpp.h")
endif()
if(NOT EXISTS "${DPP_EXTRA_INCLUDE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "nlohmann/json.hpp not found at ${DPP_EXTRA_INCLUDE_DIR}/nlohmann/json.hpp")
endif()
message(STATUS "D++ include directory: ${DPP_INCLUDE_DIR}")
message(STATUS "Additional include for nlohmann: ${DPP_EXTRA_INCLUDE_DIR}")

# 3. Locate library and required DLLs
file(GLOB DPP_LIB_FILE "${DPP_ROOT}/lib/*/dpp.lib")
if(NOT DPP_LIB_FILE)
    message(FATAL_ERROR "dpp.lib not found inside ${DPP_ROOT}/lib/*/")
endif()
list(GET DPP_LIB_FILE 0 DPP_LIB_PATH)
message(STATUS "Using library: ${DPP_LIB_PATH}")

# Collect all DLLs to copy
set(DPP_DLL_PATHS "")
file(GLOB DPP_DLL_FILE "${DPP_ROOT}/bin/dpp.dll")
if(DPP_DLL_FILE)
    list(GET DPP_DLL_FILE 0 DPP_DLL_PATH)
    list(APPEND DPP_DLL_PATHS "${DPP_DLL_PATH}")
    message(STATUS "Found DLL: ${DPP_DLL_PATH}")
else()
    message(FATAL_ERROR "dpp.dll not found inside ${DPP_ROOT}/bin/")
endif()

foreach(dll_name libcrypto-1_1-x64 libssl-1_1-x64 opus zlib1)
    file(GLOB DLL_PATH "${DPP_ROOT}/bin/${dll_name}.dll")
    if(DLL_PATH)
        list(GET DLL_PATH 0 DLL_PATH)
        list(APPEND DPP_DLL_PATHS "${DLL_PATH}")
        message(STATUS "Found DLL: ${DLL_PATH}")
    else()
        message(WARNING "${dll_name}.dll not found – audio features may not work")
    endif()
endforeach()

# ─── Source files ──────────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp")

# ─── Executable ────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

# Add both include directories: one for dpp/... and one for nlohmann/...
target_include_directories(${PROJECT_NAME} PRIVATE
    "${DPP_INCLUDE_DIR}"
    "${DPP_EXTRA_INCLUDE_DIR}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE "${DPP_LIB_PATH}")

# ─── Copy all required DLLs to output directory ────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DPP_DLL_PATHS}
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMENT "Copying D++ DLLs to target folder..."
)

# ─── Output directories (mirrors Makefile structure) ──────────
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/release"
)

# ─── Custom run targets ────────────────────────────────────────
add_custom_target(run_debug
    COMMAND ${CMAKE_BINARY_DIR}/debug/${PROJECT_NAME}.exe
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running DEBUG build..."
)

add_custom_target(run_release
    COMMAND ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}.exe
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running RELEASE build..."
)

# ─── Copy config.json next to the binary ──────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json"
)