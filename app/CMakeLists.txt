add_executable(matterthreads Main.cpp)
target_link_libraries(matterthreads PRIVATE mt_matter mt_fault mt_metrics nlohmann_json::nlohmann_json)

if(ENABLE_HW_BRIDGE)
    target_link_libraries(matterthreads PRIVATE mt_hw)
endif()
