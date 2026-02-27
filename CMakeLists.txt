cmake_minimum_required(VERSION 4.1)
project(compat55 LANGUAGES C)

# Use static CRT on MSVC (/MT) to avoid MSVCRT.lib dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Lua 5.5 sources (exclude CLI and single-file build)
file(GLOB LUA55_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lua55/*.c)
list(FILTER LUA55_SOURCES EXCLUDE REGEX "/(lua|onelua)\\.c$")

# compat55 = lua55 + compat shim (compiled as C despite .cpp extension)
add_library(compat55 STATIC ${LUA55_SOURCES} compat/lua55_compat.cpp)
set_source_files_properties(compat/lua55_compat.cpp PROPERTIES LANGUAGE C)
target_include_directories(compat55 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lua55
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Force forward-slash directory separator on all platforms (Lua 5.1 compat)
target_compile_definitions(compat55 PRIVATE "LUA_DIRSEP=\"/\"")

# Platform configuration
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(compat55 PRIVATE LUA_USE_LINUX)
    target_link_libraries(compat55 INTERFACE m dl)
elseif(APPLE AND NOT IOS)
    target_compile_definitions(compat55 PRIVATE LUA_USE_MACOSX)
    target_link_libraries(compat55 INTERFACE m)
elseif(APPLE)
    target_compile_definitions(compat55 PRIVATE LUA_USE_POSIX LUA_USE_IOS)
    target_link_libraries(compat55 INTERFACE m)
elseif(ANDROID)
    target_compile_definitions(compat55 PRIVATE LUA_USE_POSIX)
    target_link_libraries(compat55 INTERFACE m dl)
endif()

# Test executable (not for Emscripten)
if(NOT EMSCRIPTEN)
    option(COMPAT55_BUILD_TESTS "Build test_lua55 executable" ON)
    if(COMPAT55_BUILD_TESTS)
        add_executable(test_lua55 compat_tests/main.c compat_tests/lua_utf8/lutf8lib.c)
        target_include_directories(test_lua55 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lua51/src)
        target_link_libraries(test_lua55 PRIVATE compat55)
    endif()
endif()
