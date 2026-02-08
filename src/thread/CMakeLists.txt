add_library(mt_thread STATIC
    ThreadNode.cpp
    MeshTopology.cpp
    MLE.cpp
    Routing.cpp
    AddressManager.cpp
    Leader.cpp
    ChildTable.cpp
    BorderRouter.cpp
    SRP.cpp
    PowerManager.cpp
)

target_link_libraries(mt_thread PUBLIC mt_net)
target_include_directories(mt_thread PUBLIC ${CMAKE_SOURCE_DIR}/src)
