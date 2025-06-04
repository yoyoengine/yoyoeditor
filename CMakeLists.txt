# This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
# Copyright (C) 2023-2025  Ryan Zmuda

# Licensed under the MIT license. See LICENSE file in the project root for details.


# Minimum CMake version and project name
cmake_minimum_required(VERSION 3.22.1)
project(YoyoEditor)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

# set YOYO_CMAKE_COPY_ENGINE_RESOURCES to true/on since editor needs the engine resources loose folder
set(YOYO_CMAKE_COPY_ENGINE_RESOURCES true CACHE BOOL "Copy the engine resources to the editor build directory")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# configure building the lua API
# set(BUILD_LUA_RUNTIME ON CACHE BOOL "Build the Lua runtime for the editor")
# set(LUA_RUNTIME_OUTPUT ${CMAKE_BINARY_DIR}/bin/${CMAKE_SYSTEM_NAME}/engine_resources/ye_runtime.lua CACHE STRING "The output path for the Lua runtime")

# Set C/C++ compiler flags
if(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

# Define the source files for your project
file(GLOB_RECURSE SOURCES CMAKE_CONFIGURE_DEPENDS "src/*.c")

# Define the output executable name and directory
set(EXECUTABLE_NAME yoyoeditor)
set(EDITOR_BUILD_DIR "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# set rpath for Linux (will allow the game to find the engine shared library on every system)
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
SET(CMAKE_INSTALL_RPATH $ORIGIN)
# SET(CMAKE_INSTALL_RPATH $ORIGIN/lib)
# note: if you check ldd, it will still show absolute!!
#       Use chrpath to see the real rpath.
    
# Create the executable and link the engine library
add_executable(${EXECUTABLE_NAME} ${SOURCES})
    
# we take in ZOOGIES_DEVELOPMENT_BUILD if we are
# building locally outside of a normal release cycle
if(CMAKE_BUILD_TYPE STREQUAL "Debug") # FIXME: is this what we really want?
    # add_compile_definitions(ZOOGIES_DEVELOPMENT_BUILD) FIXME: redundant i think

    # this will only matter if we dont also pass YOYO_ENGINE_PATH,
    # but that would be highly abnormal for a dev build since usually
    # we are working on a dirty copy of the engine
    set(ENGINE_GIT_TAG "main" CACHE STRING "")

    # for dev builds, build with maximum warnings and error them
    if(UNIX)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wextra -Werror)
    else()
        target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall)
    endif()
else()
    set(ENGINE_GIT_TAG "build-0" CACHE STRING "")
endif()

# jannson changes
# if(WIN32)
#     set(HAVE_INT32_T 1)
#     set(HAVE_UINT32_T 1)
#     set(HAVE_UINT16_T 1)
# endif()

# Check if YOYOENGINE_PATH is defined
if(NOT DEFINED YOYO_ENGINE_PATH)
    # Grab the engine from github
    FetchContent_Declare(
        yoyoengine
        GIT_REPOSITORY https://github.com/yoyoengine/yoyoengine.git
        GIT_TAG ${ENGINE_GIT_TAG}
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
    )
        
    # A little jank... but it works
    FetchContent_MakeAvailable(yoyoengine)
    set(YOYO_ENGINE_PATH ${yoyoengine_SOURCE_DIR}/engine)
else()
    # make YOYO_ENGINE_PATH absolute path relative to the current src dir
    get_filename_component(YOYO_ENGINE_PATH ${YOYO_ENGINE_PATH} ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})

    # print the engine path
    message(STATUS "Using engine path: ${YOYO_ENGINE_PATH}")

    # Check if the engine path exists
    if(NOT EXISTS ${YOYO_ENGINE_PATH} OR NOT IS_DIRECTORY ${YOYO_ENGINE_PATH} OR NOT EXISTS ${YOYO_ENGINE_PATH}/CMakeLists.txt)
        message(FATAL_ERROR "The engine path does not exist or is not a directory. Please check the path and try again.")
    endif()
endif()

add_subdirectory(${YOYO_ENGINE_PATH} ${CMAKE_BINARY_DIR}/yoyoengine)

if(MSVC)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE yoyoengine)
else()
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE yoyoengine m)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

###############
#   libcurl   #
###############

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(CURL_STATICLIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_11_1
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(curl)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE CURL::libcurl)
if(WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE CURL_STATICLIB)
endif()

# Set the output directory for the executable
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EDITOR_BUILD_DIR}"
)

# Post build commands to copy files and create directories
add_custom_command(
    TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/settings.yoyo" "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/settings.yoyo"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/editor.yoyo" "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/editor.yoyo"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/editor_resources" "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/editor_resources"
)