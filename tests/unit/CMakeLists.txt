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

# Hardware bridge tests (optional)
if(ENABLE_HW_BRIDGE)
    add_executable(mt_hw_tests
        TestProcessManager.cpp
        TestChipToolOutputParser.cpp
        TestChipToolDriver.cpp
        TestOTBRClient.cpp
        TestHardwareNode.cpp
    )

    target_link_libraries(mt_hw_tests PRIVATE
        mt_hw
        GTest::gtest_main
    )

    gtest_discover_tests(mt_hw_tests)
endif()

# Fleet gateway tests (optional)
if(ENABLE_GATEWAY)
    add_executable(mt_gateway_tests
        TestVanRegistry.cpp
        TestOfflineBuffer.cpp
        TestFabricManager.cpp
        TestSessionPool.cpp
        TestFleetSubscription.cpp
        TestCommandRelay.cpp
        TestGatewayServer.cpp
    )

    target_link_libraries(mt_gateway_tests PRIVATE
        mt_gateway
        GTest::gtest_main
    )

    gtest_discover_tests(mt_gateway_tests)
endif()
