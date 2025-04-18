cmake_minimum_required(VERSION 3.30)
project(cell_sim)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -funroll-loops")

# Dependencies
find_package(TBB QUIET)
if (NOT TBB_FOUND)
    if (WIN32)
        message(FATAL_ERROR "TBB not found - please install Intel oneAPI Threading Building Blocks")
    endif ()
    message(STATUS "TBB not found - will download and build from source")

    FetchContent_Declare(
            tbb
            GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
            GIT_TAG v2022.0.0
    )

    set(TBB_TEST OFF)
    set(TBB_EXAMPLES OFF)

    FetchContent_MakeAvailable(tbb)
endif ()

set(RAYLIB_VERSION 5.5)
if (UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(PLATFORM "SDL" CACHE STRING "Platform for raylib")
else()
    set(PLATFORM "Desktop" CACHE STRING "Platform for raylib")
endif()

FetchContent_Declare(
        raylib
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
        FIND_PACKAGE_ARGS
)

FetchContent_Declare(
        raygui
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
        FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(raylib raygui)

# Add include path for raygui
include_directories(${raygui_SOURCE_DIR}/src)

add_executable(${PROJECT_NAME} src/main.cpp
        src/MeshGenerator.h
        src/rlights.h
        src/OctahedronGrid.h
        src/TruncatedOctahedraManager.h
        src/BoundaryManager.h
        src/TransformData.h
        src/StemCellGUI.h
)
add_subdirectory(src)

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

#set(raylib_VERBOSE 1)
if (UNIX)
    target_compile_options(tbb PRIVATE -Wno-stringop-overflow)
endif ()
target_link_libraries(${PROJECT_NAME}
        raylib
        TBB::tbb
)

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif ()