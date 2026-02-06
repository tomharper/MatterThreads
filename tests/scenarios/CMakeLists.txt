add_executable(mt_scenario_tests
    ScenarioRunner.cpp
    TestMeshHealing.cpp
    TestSubscriptionRecovery.cpp
    TestCommissioning.cpp
    TestRouteFailure.cpp
    TestMessageOrdering.cpp
)

target_link_libraries(mt_scenario_tests PRIVATE
    mt_matter
    mt_fault
    mt_metrics
    GTest::gtest_main
)

gtest_discover_tests(mt_scenario_tests)
