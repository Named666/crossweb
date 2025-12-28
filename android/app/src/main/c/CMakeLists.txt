cmake_minimum_required(VERSION 3.10.2)

project("crossweb" C)

# Set C standard to C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories("../../../../build")
# Define for Android
add_definitions(-DCROSSWEB_TARGET_ANDROID=1)

add_library(crossweb SHARED
    android_bridge.c
    plug.c
    ipc.c
)

find_library(log-lib log)

target_link_libraries(crossweb ${log-lib})