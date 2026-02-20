add_library(veloz_strategy
    src/strategy_manager.cpp
    src/advanced_strategies.cpp
    src/trend_following_strategy.cpp
    src/mean_reversion_strategy.cpp
    src/momentum_strategy.cpp
    src/market_making_strategy.cpp
    src/grid_strategy.cpp
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

# Tests using KJ test framework
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_strategy_tests
        tests/test_strategy_manager.cpp
        tests/test_advanced_strategies.cpp
        tests/test_trend_following_strategy.cpp
        tests/test_mean_reversion_strategy.cpp
        tests/test_market_making_strategy.cpp
        tests/test_momentum_strategy.cpp
        tests/test_grid_strategy.cpp
        tests/test_strategy_impl.cpp
    )

    target_link_libraries(veloz_strategy_tests
        PRIVATE
            veloz_strategy
            veloz_core
            veloz_market
            veloz_exec
            veloz_oms
            veloz_risk
            kj-test
            kj-async
    )
    add_test(NAME veloz_strategy_tests COMMAND veloz_strategy_tests)
endif()
