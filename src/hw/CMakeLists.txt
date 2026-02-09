find_package(CURL REQUIRED)

add_library(mt_hw STATIC
    ProcessManager.cpp
    ChipToolOutputParser.cpp
    ChipToolDriver.cpp
    OTBRClient.cpp
    HardwareNode.cpp
)

target_link_libraries(mt_hw PUBLIC mt_matter CURL::libcurl nlohmann_json::nlohmann_json)
target_include_directories(mt_hw PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Optional: configure chip-tool binary path
set(CHIP_TOOL_PATH "" CACHE FILEPATH "Path to chip-tool binary")
if(CHIP_TOOL_PATH)
    target_compile_definitions(mt_hw PRIVATE CHIP_TOOL_DEFAULT_PATH="${CHIP_TOOL_PATH}")
endif()

# Darwin bridge (macOS/iOS only)
if(APPLE AND ENABLE_DARWIN_BRIDGE)
    add_subdirectory(darwin)
endif()
