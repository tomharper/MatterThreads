add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(thread)
add_subdirectory(matter)
add_subdirectory(fault)
add_subdirectory(metrics)
add_subdirectory(broker)
add_subdirectory(node)

if(ENABLE_HW_BRIDGE)
    add_subdirectory(hw)
    # Propagate to app targets that need to know hw bridge is available
    add_compile_definitions(MT_HW_BRIDGE_ENABLED)
endif()

if(ENABLE_GATEWAY)
    add_subdirectory(gateway)
    add_compile_definitions(MT_GATEWAY_ENABLED)
endif()
