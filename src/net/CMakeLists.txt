add_library(mt_net STATIC
    Socket.cpp
    Frame.cpp
    Channel.cpp
    Broker.cpp
    FaultInjector.cpp
)

target_link_libraries(mt_net PUBLIC mt_core nlohmann_json::nlohmann_json)
target_include_directories(mt_net PUBLIC ${CMAKE_SOURCE_DIR}/src)
