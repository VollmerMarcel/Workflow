add_executable(carwear_tests
    test_rng.cpp
    test_date.cpp
    test_config.cpp
    test_physics.cpp
    test_simulation.cpp
    test_statistics.cpp
    test_output.cpp
)

target_link_libraries(carwear_tests PRIVATE carwear_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(carwear_tests)
