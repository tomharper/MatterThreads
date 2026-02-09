# Darwin Matter Framework bridge
# Requires Matter.framework from connectedhomeip Darwin build

add_library(mt_hw_darwin STATIC
    DarwinBridge.mm
)

target_link_libraries(mt_hw_darwin PUBLIC
    mt_hw
    "-framework Foundation"
    "-framework Security"
)

# Look for Matter.framework in connectedhomeip build output
find_library(MATTER_FRAMEWORK Matter HINTS
    ${CMAKE_SOURCE_DIR}/third_party/connectedhomeip/out/darwin-x86_64-framework
    ${CMAKE_SOURCE_DIR}/third_party/connectedhomeip/out/darwin-arm64-framework
    ${CMAKE_SOURCE_DIR}/third_party/connectedhomeip/out/darwin-framework
)
if(MATTER_FRAMEWORK)
    target_link_libraries(mt_hw_darwin PUBLIC ${MATTER_FRAMEWORK})
    target_compile_definitions(mt_hw_darwin PRIVATE HAS_MATTER_FRAMEWORK=1)
else()
    message(WARNING "Matter.framework not found — Darwin bridge will compile but not link to real SDK")
endif()

target_compile_options(mt_hw_darwin PRIVATE
    -fobjc-arc
    -Wno-deprecated-declarations
    -Wno-sign-conversion
    -Wno-conversion
)

target_include_directories(mt_hw_darwin PUBLIC ${CMAKE_SOURCE_DIR}/src)
