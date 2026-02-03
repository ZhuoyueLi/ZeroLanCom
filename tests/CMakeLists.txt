# ZeroLanCom Test Suite
# Requires Google Test (GTest)

cmake_minimum_required(VERSION 3.14)

# ----------------------------
# Find GTest
# ----------------------------
find_package(GTest REQUIRED)
include(GoogleTest)

# ----------------------------
# Test include directories
# ----------------------------
set(TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ----------------------------
# Helper function to create test targets
# ----------------------------
function(add_zerolancom_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE
        ${TEST_INCLUDE_DIR}
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        zerolancom
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(${TEST_NAME})
endfunction()

# ----------------------------
# Unit Tests
# ----------------------------
add_zerolancom_test(test_serialization test_serialization.cpp)

# ----------------------------
# Integration Tests (require singleton reset)
# ----------------------------
add_zerolancom_test(test_single_node test_single_node.cpp)
add_zerolancom_test(test_service test_service.cpp)
add_zerolancom_test(test_pubsub test_pubsub.cpp)
