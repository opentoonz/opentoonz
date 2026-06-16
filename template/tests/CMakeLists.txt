# Test executable
add_executable(yz-test
    test_main.cpp
)

target_include_directories(yz-test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(yz-test PRIVATE
    gtest_main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(yz-test)
