add_executable(mt_tests
    TestTLV.cpp
    TestRouting.cpp
    TestChannel.cpp
    TestInteractionModel.cpp
    TestFaultInjector.cpp
    TestDiscovery.cpp
    TestSelfHealing.cpp
    TestBorderRouter.cpp
    TestSRP.cpp
    TestPowerManager.cpp
    TestDashboardServer.cpp
)

target_link_libraries(mt_tests PRIVATE
    mt_matter
    mt_fault
    mt_metrics
    GTest::gtest_main
)

gtest_discover_tests(mt_tests)
