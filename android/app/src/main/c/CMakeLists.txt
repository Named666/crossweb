cmake_minimum_required(VERSION 3.4.1)
project(crossweb C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Add your native source files here
add_library(
	crossweb
	SHARED
	android_bridge.c
	ipc.c
	plug.c
	plugins/fs/commands.c
	plugins/fs/error.c
	plugins/fs/lib.c
	plugins/keystore/plugin.c
)

# Link against Android log library
find_library(
	log-lib
	log
)
target_link_libraries(
	crossweb
	${log-lib}
)
