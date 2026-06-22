cmake_minimum_required(VERSION 3.10)
project(Elaina)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5.0)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
endif()

include(FetchContent)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

FetchContent_Declare(
    blake3
    GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
    GIT_TAG 1.8.2
    SOURCE_SUBDIR c
)
set(BLAKE3_FETCH_CONTENT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(blake3)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
)
set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/Roblox/luau.git
    GIT_TAG 0.663
    SOURCE_SUBDIR .
)
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(luau)

file(GLOB_RECURSE CORE_SOURCES
    "src/core/*.cpp"
    "src/syscall/*.cpp" "src/syscall/*.asm"
    "src/process/*.cpp"
    "src/instance/*.cpp"
    "src/execution/*.cpp"
    "src/network/*.cpp"
    "src/service/*.cpp"
    "src/api/*.cpp"
)

set_source_files_properties("src/syscall/Syscall.asm" PROPERTIES LANGUAGE ASM_MASM)
enable_language(ASM_MASM)

include_directories(src)

add_library(Elaina SHARED ${CORE_SOURCES})
target_compile_definitions(Elaina PRIVATE ELAINA_EXPORT WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(Elaina PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(Elaina PRIVATE
    kernel32 user32 winhttp ws2_32
    Luau.Compiler Luau.Ast
    zlibstatic libzstd_static blake3
    httplib::httplib nlohmann_json::nlohmann_json
)

target_include_directories(Elaina PRIVATE
    ${luau_SOURCE_DIR}/Compiler/include
    ${luau_SOURCE_DIR}/Ast/include
    ${luau_SOURCE_DIR}/Common/include
    ${zlib_SOURCE_DIR}
    ${zlib_BINARY_DIR}
    ${zstd_SOURCE_DIR}/lib
    ${zstd_SOURCE_DIR}/lib/common
    ${blake3_SOURCE_DIR}/c
)
