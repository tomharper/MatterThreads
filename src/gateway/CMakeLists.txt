add_library(mt_gateway STATIC
    VanRegistry.cpp
    OfflineBuffer.cpp
    FabricManager.cpp
    SessionPool.cpp
    FleetSubscriptionManager.cpp
    CommandRelay.cpp
    GatewayServer.cpp
)

target_link_libraries(mt_gateway PUBLIC mt_hw mt_metrics nlohmann_json::nlohmann_json)
target_include_directories(mt_gateway PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(mt_gateway_server GatewayMain.cpp)
target_link_libraries(mt_gateway_server PRIVATE mt_gateway)
