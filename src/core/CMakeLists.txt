add_library(mt_core STATIC
    Clock.cpp
    Log.cpp
    Random.cpp
)

target_include_directories(mt_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
