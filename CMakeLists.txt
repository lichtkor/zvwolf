cmake_minimum_required(VERSION 3.20)

project(zvwolf VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "Supported build types" FORCE)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
    set(GLFW_TARGET glfw3::glfw)
else()
    message(FATAL_ERROR "GLFW target not found")
endif()

add_executable(zvwolf
    src/zvwolf.cpp
    src/tinygl.cpp
    third_party/glad/glad.c
)

target_include_directories(zvwolf PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/glad
    ${PROJECT_SOURCE_DIR}/third_party/stb
)

target_link_libraries(zvwolf PRIVATE
    ${GLFW_TARGET}
    OpenGL::GL
)

if(WIN32)
    enable_language(RC)
    target_sources(zvwolf PRIVATE
        resources/zvwolf.rc
    )
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set_property(TARGET zvwolf PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET zvwolf PROPERTY INTERPROCEDURAL_OPTIMIZATION_DIST TRUE)
endif()

target_compile_options(zvwolf PRIVATE -Wall -Wextra)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   -O3 -march=native -flto")

set(CMAKE_CXX_FLAGS_DIST    "${CMAKE_CXX_FLAGS_DIST}    -O3 -flto -static-libgcc -static-libstdc++")
set(CMAKE_C_FLAGS_DIST      "${CMAKE_C_FLAGS_DIST}      -O3 -flto -static-libgcc")

set_target_properties(zvwolf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

add_custom_command(TARGET zvwolf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:zvwolf>/assets"

    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:zvwolf>/resources"

    COMMAND ${CMAKE_COMMAND} -E copy
        "${PROJECT_SOURCE_DIR}/resources/zvwolf.png"
        "$<TARGET_FILE_DIR:zvwolf>/resources/zvwolf.png"

    COMMENT "Copying ZVWolf assets..."
)