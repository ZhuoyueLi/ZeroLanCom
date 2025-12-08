cmake_minimum_required(VERSION 3.14)
project(ZEROLANCOM LANGUAGES CXX)

# ----------------------------
# C++ Standard
# ----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------
# Debug settings
# ----------------------------
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ----------------------------
# External dependencies
# ----------------------------
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
# find_package(msgpack-cpp REQUIRED)

# ----------------------------
# Include directory
# ----------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)

# ----------------------------
# Source files
# ----------------------------
# file(GLOB_RECURSE ZEROLANCOM_SOURCES
#     "${PROJECT_SOURCE_DIR}/src/*.cpp"
# )

# ----------------------------
# Build C++ core library
# ----------------------------
add_library(zerolancom INTERFACE)
# target_include_directories(zerolancom INTERFACE
#     ${PROJECT_SOURCE_DIR}/include
# )
target_include_directories(zerolancom INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(zerolancom INTERFACE
    ${ZMQ_LIBRARIES}
    spdlog::spdlog
    msgpackc
)

# ----------------------------
# Install library
# ----------------------------
install(TARGETS zerolancom
        EXPORT zerolancomTargets)

# Install header files
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION include/zerolancom)

# Export CMake targets to be findable
install(EXPORT zerolancomTargets
        FILE zerolancomTargets.cmake
        NAMESPACE zlc::
        DESTINATION lib/cmake/zerolancom)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zerolancomConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/zerolancomConfig.cmake
    INSTALL_DESTINATION lib/cmake/zerolancom
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zerolancomConfig.cmake
    DESTINATION lib/cmake/zerolancom
)

# ----------------------------
# Add examples
# ----------------------------
add_subdirectory(examples)