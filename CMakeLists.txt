cmake_minimum_required(VERSION 3.3)
project(Slade3 VERSION 3.2.2)

if(COMMAND cmake_policy)
	cmake_policy( SET CMP0012 NEW )
	cmake_policy( SET CMP0003 NEW )
	cmake_policy( SET CMP0005 NEW )
	cmake_policy( SET CMP0011 NEW )
	if (POLICY CMP0072)
	cmake_policy( SET CMP0072 NEW )
	endif(POLICY CMP0072)
endif(COMMAND cmake_policy)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(SLADE_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory where slade will be created.")

OPTION(NO_WEBVIEW "Disable wxWebview usage (for start page and documentation)" OFF)
OPTION(USE_SFML_RENDERWINDOW "Use SFML RenderWindow for OpenGL displays" OFF)
if(NOT APPLE)
	OPTION(WX_GTK3 "Use GTK3 (if wx is built with it)" ON)
endif(NOT APPLE)

# c++17 is required to compile
if(CMAKE_VERSION VERSION_LESS 3.1)
	add_compile_options(-std=c++17)
else()
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
endif()

add_subdirectory(src)
add_subdirectory(dist)
