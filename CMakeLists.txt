cmake_minimum_required (VERSION 2.6)
project (nes-emu)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # Multiprocessor build
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX") # Max warning level and error on warnings
	if(MSVC_VERSION LESS 1900) # Starting from MSVC 14 (2015), STL needs language extensions enabled
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za") # Disable language extensions
	endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# To find SDL2 using find_package below, we set the module path to a directory containing
# the FindSDL2.cmake module script.
# NOTE: On Windows, set "SDL2" environment variable to the path to the SDL2 development libraries root
#       which can be downloaded here: https://www.libsdl.org/download-2.0.php
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Look up SDL2 and add the include directory to our include path
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

file(GLOB SRC "src/*.cpp")
add_executable(nes-emu ${SRC})
target_link_libraries(nes-emu ${SDL2_LIBRARY})

# For VS, add post-build step to copy SDL2.dll to the output directory
if(MSVC)
	add_custom_command(TARGET nes-emu POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_if_different
						"${SDL2_INCLUDE_DIR}/../lib/x86/SDL2.DLL" $<TARGET_FILE_DIR:nes-emu>)
endif()
