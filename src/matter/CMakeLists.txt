add_library(mt_matter STATIC
    TLV.cpp
    Session.cpp
    PASE.cpp
    CASE.cpp
    Fabric.cpp
    InteractionModel.cpp
    SubscriptionManager.cpp
    DataModel.cpp
    Exchange.cpp
)

target_link_libraries(mt_matter PUBLIC mt_thread)
target_include_directories(mt_matter PUBLIC ${CMAKE_SOURCE_DIR}/src)
