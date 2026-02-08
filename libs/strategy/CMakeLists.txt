add_library(veloz_strategy
    src/strategy_manager.cpp
    src/advanced_strategies.cpp
    src/test_strategy.cpp
)

target_include_directories(veloz_strategy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_strategy
    PUBLIC
        veloz_core
        veloz_market
        veloz_exec
        veloz_oms
        veloz_risk
)

target_compile_features(veloz_strategy PUBLIC cxx_std_23)

# Tests
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_strategy_tests
        tests/test_strategy_manager.cpp
        tests/test_advanced_strategies.cpp
    )

    target_link_libraries(veloz_strategy_tests
        PRIVATE
            veloz_strategy
            veloz_core
            veloz_market
            veloz_exec
            veloz_oms
            veloz_risk
            GTest::gtest
            GTest::gtest_main
    )

    gtest_discover_tests(veloz_strategy_tests)
endif()
