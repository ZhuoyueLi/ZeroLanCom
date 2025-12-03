cmake_minimum_required(VERSION 3.14)
project(LANCOM_CPP LANGUAGES CXX)

# ----------------------------
# C++ Standard
# ----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ----------------------------
# Compile options
# ----------------------------
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# ----------------------------
# External dependencies
# ----------------------------
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)


# ----------------------------
# Include directory
# ----------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)

# ----------------------------
# Source files
# ----------------------------
file(GLOB_RECURSE LANCOM_SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# ----------------------------
# Build the library
# ----------------------------
add_library(lancom ${LANCOM_SOURCES})

target_include_directories(lancom PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(main ${PROJECT_SOURCE_DIR}/src/lancom.cpp)
target_include_directories(main PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(main PRIVATE ${ZMQ_LIBRARIES} spdlog::spdlog)

# # ----------------------------
# # Examples (optional)
# # ----------------------------
# option(LANCOM_BUILD_EXAMPLES "Build LANCOM examples" ON)
# if (LANCOM_BUILD_EXAMPLES)
#     message(STATUS "Building examples...")

#     file(GLOB EXAMPLE_SRCS "${PROJECT_SOURCE_DIR}/examples/*.cpp")

#     foreach(example_file ${EXAMPLE_SRCS})
#         get_filename_component(example_name ${example_file} NAME_WE)

#         add_executable(${example_name} ${example_file})
#         target_link_libraries(${example_name} PRIVATE lancom)
#     endforeach()
# endif()

# # ----------------------------
# # Tests (optional)
# # ----------------------------
# option(LANCOM_BUILD_TESTS "Build LANCOM tests" OFF)
# if (LANCOM_BUILD_TESTS)
#     enable_testing()

#     file(GLOB TEST_SRCS "${PROJECT_SOURCE_DIR}/tests/*.cpp")

#     foreach(test_file ${TEST_SRCS})
#         get_filename_component(test_name ${test_file} NAME_WE)

#         add_executable(${test_name} ${test_file})
#         target_link_libraries(${test_name} PRIVATE lancom)

#         add_test(NAME ${test_name} COMMAND ${test_name})
#     endforeach()
# endif()
