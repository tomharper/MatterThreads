add_library(mt_fault STATIC
    FaultPlan.cpp
    Scheduler.cpp
    Chaos.cpp
)

target_link_libraries(mt_fault PUBLIC mt_matter nlohmann_json::nlohmann_json)
target_include_directories(mt_fault PUBLIC ${CMAKE_SOURCE_DIR}/src)
