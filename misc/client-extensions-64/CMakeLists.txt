# CMakeLists.txt : CMake project for ClientExtensions64, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ClientExtensions64")

option(DUSKHAVEN_ENABLE_FMOD_VOICE "Enable Discord proximity voice through FMOD Core" ON)
set(FMOD_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FMOD" CACHE PATH "FMOD Studio/Core SDK root")

set(DUSKHAVEN_HAVE_FMOD_VOICE OFF)
if(DUSKHAVEN_ENABLE_FMOD_VOICE)
    find_path(FMOD_INCLUDE_DIR
        NAMES fmod.hpp
        PATHS "${FMOD_SDK_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/FMOD" "$ENV{FMOD_SDK_DIR}"
        PATH_SUFFIXES api/core/inc inc
        NO_DEFAULT_PATH
    )
    find_library(FMOD_LIBRARY
        NAMES fmod_vc fmod
        PATHS "${FMOD_SDK_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/FMOD" "$ENV{FMOD_SDK_DIR}"
        PATH_SUFFIXES api/core/lib/x64 api/core/lib lib x64
        NO_DEFAULT_PATH
    )
    find_file(FMOD_SHARED_LIB
        NAMES fmod.dll libfmod.so libfmod.dylib
        PATHS "${FMOD_SDK_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/FMOD" "$ENV{FMOD_SDK_DIR}"
        PATH_SUFFIXES api/core/lib/x64 api/core/bin/x64 api/core/lib lib x64
        NO_DEFAULT_PATH
    )

    if(FMOD_INCLUDE_DIR AND FMOD_LIBRARY)
        set(DUSKHAVEN_HAVE_FMOD_VOICE ON)
        message(STATUS "FMOD proximity voice enabled")
    else()
        message(WARNING "FMOD SDK not found. Expected ${CMAKE_CURRENT_SOURCE_DIR}/FMOD or set FMOD_SDK_DIR.")
    endif()
endif()

# Collect all sources/headers recursively
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/discord_social_sdk/*.cpp"
    "${CMAKE_SOURCE_DIR}/discord_social_sdk/*.h"
)

# Exclude anything in lib_test/
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/discord_social_sdk/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/out/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/.vs/.*")

# Add source to this project's executable.
add_executable(ClientExtensions64 ${PROJECT_SOURCES})


# Define some handy Social SDK variables
set(DISCORD_SDK_ROOT "${CMAKE_SOURCE_DIR}/discord_social_sdk")
set(DISCORD_SDK_LIB_DIR "${DISCORD_SDK_ROOT}/lib/release")
set(DISCORD_SDK_BIN_DIR "${DISCORD_SDK_ROOT}/bin/release")
set(DISCORD_SDK_INCLUDE_DIR "${DISCORD_SDK_ROOT}/include")

# Include for Social SDK headers
target_include_directories(ClientExtensions64 PRIVATE ${DISCORD_SDK_INCLUDE_DIR})
target_compile_definitions(ClientExtensions64 PRIVATE DUSKHAVEN_ENABLE_FMOD_VOICE=1)
target_include_directories(ClientExtensions64 PRIVATE ${FMOD_INCLUDE_DIR})
target_link_libraries(ClientExtensions64 PRIVATE ${FMOD_LIBRARY})

# Platform-specific Social SDK library paths
if(WIN32)
    set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/discord_partner_sdk.lib")
    set(DISCORD_SHARED_LIB "${DISCORD_SDK_BIN_DIR}/discord_partner_sdk.dll")
elseif(APPLE)
    set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.dylib")
    set(DISCORD_SHARED_LIB "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.dylib")
else() # Linux
    set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")
    set(DISCORD_SHARED_LIB "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")
endif()

# Link the Social SDK library
target_link_libraries(ClientExtensions64 PRIVATE ${DISCORD_LIB_PATH})
if(WIN32)
    target_link_libraries(ClientExtensions64 PRIVATE Crypt32 Ole32)
endif()

# Set the runtime search path (RPATH) for Linux and macOS
if(UNIX)
    # Use RPATH when building
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    # Set the RPATH to use the lib directory relative to the executable
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@executable_path")
    endif()
endif()

# Copy Social SDK shared library to output directory, so it's available at runtime.
add_custom_command(TARGET ClientExtensions64 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DISCORD_SHARED_LIB}"
    $<TARGET_FILE_DIR:ClientExtensions64>;
)

add_custom_command(TARGET ClientExtensions64 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_SHARED_LIB}"
    $<TARGET_FILE_DIR:ClientExtensions64>;
    
