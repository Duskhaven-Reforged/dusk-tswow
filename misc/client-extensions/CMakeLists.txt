# project
cmake_minimum_required(VERSION 3.25)

set(CMAKE_GENERATOR_PLATFORM Win32)

cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 17)

project(ClientExtensions)

set(DETOURS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/Detours")
set(LUA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/lua-5.1")

file(GLOB_RECURSE Detours_sources CONFIGURE_DEPENDS
    "${DETOURS_SOURCE_DIR}/src/*.c"
    "${DETOURS_SOURCE_DIR}/src/*.h"
    "${DETOURS_SOURCE_DIR}/src/*.cpp"
    "${DETOURS_SOURCE_DIR}/src/*.hpp"
    "${DETOURS_SOURCE_DIR}/src/*.inl"
)

set_property(
    SOURCE "${DETOURS_SOURCE_DIR}/src/uimports.cpp"
    APPEND PROPERTY HEADER_FILE_ONLY true
)
add_library(detours STATIC ${Detours_sources})
target_compile_options(detours PRIVATE /W4 /WX /Zi /MT /Gy /Gm- /Zl /Od)
target_include_directories(detours PUBLIC ${DETOURS_SOURCE_DIR})

#FILE(GLOB_RECURSE Lua_sources CONFIGURE_DEPENDS
#    "${LUA_SOURCE_DIR}/src/*.h"
#    "${LUA_SOURCE_DIR}lua-5.1/src/*.c"
#)
#add_library(lua STATIC ${Lua_sources})
#target_compile_options(lua PRIVATE -w)

file(GLOB_RECURSE ClientExtensions_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ClientExtensions_includes CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
)

list(FILTER ClientExtensions_sources EXCLUDE REGEX ".*/client_header_builder*")

add_library(ClientExtensions SHARED
    ${ClientExtensions_sources}
)

target_sources(ClientExtensions PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
    FILES ${ClientExtensions_includes}
)

target_link_libraries(ClientExtensions PRIVATE detours)

if(MSVC)
    target_compile_definitions(ClientExtensions PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(ClientExtensions PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DETOURS_SOURCE_DIR}/src
    ${LUA_SOURCE_DIR}/src
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "DLL" FILES
    ${ClientExtensions_sources}
	${ClientExtensions_includes}
)

option(LUA_LOGGING "Allow logging from client lua" ON)
if(LUA_LOGGING)
    target_compile_definitions(ClientExtensions PRIVATE LUA_LOGGING)
endif()

set(CLIENT_PATH "" CACHE PATH "Path to your wow client")
if(NOT CLIENT_PATH STREQUAL "")
    add_custom_command(
        TARGET ClientExtensions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ClientExtensions>
        ${CLIENT_PATH}
    )
    message("ClientExtensions.dll will be written to ${CLIENT_PATH} on build")
endif()

# client-header-builder
add_executable(client-header-builder client_header_builder.cpp)
set_property(TARGET client-header-builder PROPERTY CXX_STANDARD 17)

add_custom_target(client-header-builder-run
    COMMAND client-header-builder
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
    COMMENT "Generating client lua headers"
)

add_dependencies(ClientExtensions client-header-builder-run)
add_dependencies(client-header-builder-run client-header-builder)
