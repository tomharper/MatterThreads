add_executable(mt_scenario_tests
    ScenarioRunner.cpp
    TestMeshHealing.cpp
    TestSubscriptionRecovery.cpp
    TestCommissioning.cpp
    TestRouteFailure.cpp
    TestMessageOrdering.cpp
    TestVanScenarios.cpp
    TestPowerLifecycle.cpp
)

target_link_libraries(mt_scenario_tests PRIVATE
    mt_matter
    mt_fault
    mt_metrics
    GTest::gtest_main
)

gtest_discover_tests(mt_scenario_tests)

# Certification test harness (optional, requires hardware)
if(ENABLE_HW_BRIDGE)
    add_executable(mt_cert_tests
        TestCertHarness.cpp
    )

    target_link_libraries(mt_cert_tests PRIVATE
        mt_hw
        mt_fault
        mt_metrics
        GTest::gtest_main
    )

    gtest_discover_tests(mt_cert_tests)
endif()

# Fleet gateway scenario tests (optional)
if(ENABLE_GATEWAY)
    add_executable(mt_gateway_scenario_tests
        TestGatewayScenarios.cpp
        ScenarioRunner.cpp
    )

    target_link_libraries(mt_gateway_scenario_tests PRIVATE
        mt_gateway
        mt_fault
        mt_metrics
        GTest::gtest_main
    )

    gtest_discover_tests(mt_gateway_scenario_tests)
endif()
