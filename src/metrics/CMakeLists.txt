add_library(mt_metrics STATIC
    Counter.cpp
    Histogram.cpp
    Timeline.cpp
    Collector.cpp
    Reporter.cpp
    DashboardServer.cpp
)

target_link_libraries(mt_metrics PUBLIC mt_matter nlohmann_json::nlohmann_json)
target_include_directories(mt_metrics PUBLIC ${CMAKE_SOURCE_DIR}/src)
