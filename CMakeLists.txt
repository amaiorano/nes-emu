cmake_minimum_required (VERSION 3.2)
project (nes-emu)

# To find SDL2 using find_package below, we set the module path to a directory containing
# the FindSDL2.cmake module script.
# NOTE: On Windows, set "SDL2" environment variable to the path to the SDL2 development libraries root
#       which can be downloaded here: https://www.libsdl.org/download-2.0.php
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

file(GLOB SRC "src/*.cpp" "src/*.h")
add_executable(nes-emu ${SRC})

# Look up SDL2 and add include/lib dirs to it
set(SDL2_BUILDING_LIBRARY ON) # Don't find SDL2main lib
find_package(SDL2 REQUIRED)
target_include_directories(nes-emu PRIVATE ${SDL2_INCLUDE_DIR})
target_link_libraries(nes-emu PRIVATE ${SDL2_LIBRARY})
# For VS, add post-build step to copy SDL2.dll to the output directory
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_custom_command(	TARGET nes-emu POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_if_different
						"${SDL2_INCLUDE_DIR}/../lib/x86/SDL2.DLL" $<TARGET_FILE_DIR:nes-emu>)
endif()


if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(nes-emu PRIVATE _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
	target_compile_options(nes-emu PRIVATE /MP /W4 /WX)
	if (MSVC_VERSION LESS 1900) # Starting from MSVC 14 (2015), STL needs language extensions enabled
		target_compile_options(nes-emu PRIVATE /za) # disable language extensions
	endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(nes-emu PRIVATE -std=c++11)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(nes-emu PRIVATE -std=c++11)
endif()
