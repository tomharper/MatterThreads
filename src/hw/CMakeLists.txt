find_package(CURL REQUIRED)

add_library(mt_hw STATIC
    OTBRClient.cpp
    ChipToolDriver.cpp
    HardwareNode.cpp
)

target_link_libraries(mt_hw PUBLIC mt_matter CURL::libcurl nlohmann_json::nlohmann_json)
target_include_directories(mt_hw PUBLIC ${CMAKE_SOURCE_DIR}/src)
