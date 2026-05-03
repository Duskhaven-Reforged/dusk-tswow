# project
cmake_minimum_required(VERSION 3.25)

set(CMAKE_GENERATOR_PLATFORM Win32)

cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(ClientExtensions)

set(DETOURS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/Detours")
set(LUA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/lua-5.1")
set(CUSTOM_PACKET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CustomPackets")
set(FREETYPE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/freetype-2.14.1")
set(MSDFGEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/msdfgen")
set(SKIA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/skia")
set(UNORDERED_DENSE_DEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/unordered_dense")

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

FILE(GLOB_RECURSE CUSTOM_PACKET_SOURCES CONFIGURE_DEPENDS
   "${CUSTOM_PACKET_DIR}/*.h"
   "${CUSTOM_PACKET_DIR}/*.cpp"
)
add_library(cp STATIC ${CUSTOM_PACKET_SOURCES})
target_include_directories(cp PUBLIC ${CUSTOM_PACKET_DIR})

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory("${FREETYPE_SOURCE_DIR}" EXCLUDE_FROM_ALL)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA ON CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME OFF CACHE BOOL "" FORCE)
add_subdirectory("${MSDFGEN_SOURCE_DIR}" EXCLUDE_FROM_ALL)
foreach(_msdf_target IN ITEMS msdfgen-core msdfgen-ext freetype)
    if(TARGET ${_msdf_target})
        set_property(TARGET ${_msdf_target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endforeach()

if(NOT EXISTS "${UNORDERED_DENSE_DEST_DIR}/include")
    file(GLOB UD_ZIP_CANDIDATES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/libs"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/unordered_dense*.zip")
    if(UD_ZIP_CANDIDATES)
        list(GET UD_ZIP_CANDIDATES 0 UD_ZIP_NAME)
        set(UD_ZIP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/${UD_ZIP_NAME}")
    else()
        message(FATAL_ERROR "unordered_dense zip not found in ${CMAKE_CURRENT_SOURCE_DIR}/libs")
    endif()

    set(_ud_tmp "${CMAKE_BINARY_DIR}/_ud_extract")
    file(REMOVE_RECURSE "${_ud_tmp}")
    file(MAKE_DIRECTORY "${_ud_tmp}")
    file(ARCHIVE_EXTRACT INPUT "${UD_ZIP_PATH}" DESTINATION "${_ud_tmp}")
    file(GLOB _ud_roots RELATIVE "${_ud_tmp}" "${_ud_tmp}/*")
    list(LENGTH _ud_roots _ud_root_count)
    if(_ud_root_count EQUAL 1)
        list(GET _ud_roots 0 _ud_single)
        set(_ud_single_path "${_ud_tmp}/${_ud_single}")
        if(EXISTS "${_ud_single_path}/include")
            file(RENAME "${_ud_single_path}" "${UNORDERED_DENSE_DEST_DIR}")
        else()
            file(RENAME "${_ud_tmp}" "${UNORDERED_DENSE_DEST_DIR}")
        endif()
    else()
        if(EXISTS "${_ud_tmp}/include")
            file(RENAME "${_ud_tmp}" "${UNORDERED_DENSE_DEST_DIR}")
        else()
            file(MAKE_DIRECTORY "${UNORDERED_DENSE_DEST_DIR}")
            file(GLOB _ud_all_items RELATIVE "${_ud_tmp}" "${_ud_tmp}/*")
            foreach(_it IN LISTS _ud_all_items)
                file(RENAME "${_ud_tmp}/${_it}" "${UNORDERED_DENSE_DEST_DIR}/${_it}")
            endforeach()
        endif()
    endif()
    if(EXISTS "${_ud_tmp}")
        file(REMOVE_RECURSE "${_ud_tmp}")
    endif()
endif()

if(NOT TARGET ankerl::unordered_dense)
    add_library(ankerl_unordered_dense INTERFACE)
    target_include_directories(ankerl_unordered_dense INTERFACE "${UNORDERED_DENSE_DEST_DIR}/include")
    add_library(ankerl::unordered_dense ALIAS ankerl_unordered_dense)
endif()

file(GLOB_RECURSE ClientExtensions_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE MSDF_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Rendering/MSDF/*.cpp"
)
set_source_files_properties(${MSDF_SOURCES} PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/std:c++20>;$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++20>"
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
target_link_libraries(ClientExtensions PRIVATE cp)
target_link_libraries(ClientExtensions PRIVATE
    msdfgen::msdfgen-core
    msdfgen::msdfgen-ext
    freetype
    d3d9
    d3dcompiler
    onecore
    "${SKIA_SOURCE_DIR}/skia.lib"
    ankerl::unordered_dense
)

if(MSVC)
    target_compile_definitions(ClientExtensions PUBLIC _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00)
endif()

target_include_directories(ClientExtensions PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DETOURS_SOURCE_DIR}/src
    ${LUA_SOURCE_DIR}/src
    ${CUSTOM_PACKET_DIR}
    ${FREETYPE_SOURCE_DIR}/include
    ${MSDFGEN_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/libs/msdfgen
    ${SKIA_SOURCE_DIR}/include
    ${UNORDERED_DENSE_DEST_DIR}/include
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SKIA_SOURCE_DIR}/skia.dll"
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
