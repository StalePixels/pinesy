# libpinesy: the shared core of the pinesy front ends.

cmake_minimum_required(VERSION 3.20)
project(pinesy_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The engine: libmame_pinesy, built from ../mame with the pinesy OSD in ../engine.
if(APPLE)
	set(_pinesy_engine_default "${CMAKE_CURRENT_SOURCE_DIR}/../mame/libmame_pinesy.dylib")
else()
	set(_pinesy_engine_default "${CMAKE_CURRENT_SOURCE_DIR}/../mame/libmame_pinesy.so")
endif()
set(PINESY_ENGINE_LIBRARY "${_pinesy_engine_default}" CACHE FILEPATH "The engine library, libmame_pinesy")
set(PINESY_ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../engine/include" CACHE PATH "The folder with the engine's pinesy.h")

if(NOT EXISTS "${PINESY_ENGINE_LIBRARY}")
	message(FATAL_ERROR "The engine library is not there: ${PINESY_ENGINE_LIBRARY}. Build it first, or set PINESY_ENGINE_LIBRARY.")
endif()

add_library(mame_pinesy SHARED IMPORTED)
set_target_properties(mame_pinesy PROPERTIES
	IMPORTED_LOCATION "${PINESY_ENGINE_LIBRARY}"
	INTERFACE_INCLUDE_DIRECTORIES "${PINESY_ENGINE_INCLUDE_DIR}")

add_library(pinesy SHARED
	src/c_api.cpp
	src/catalogue.cpp
	src/configuration.cpp
	src/core.cpp
	src/document.cpp
	src/json.cpp
	src/mame_data.cpp)
target_include_directories(pinesy
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pinesy PUBLIC mame_pinesy)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(pinesy PRIVATE -Wall -Wextra)
endif()

# Tests: the rules checked against the real engine.
option(PINESY_CORE_TESTS "Build the core's tests" ON)
if(PINESY_CORE_TESTS)
	enable_testing()
	add_executable(core_tests tests/core_tests.cpp)
	target_include_directories(core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_link_libraries(core_tests PRIVATE pinesy)

	# The tests use the ROM sets in ../roms (as the bundled ROMs) and a
	# data folder in the build folder, so MAME writes only there.
	get_filename_component(_pinesy_public "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
	set(PINESY_TEST_RESOURCES "${_pinesy_public}" CACHE PATH "The resources folder the tests use; its roms folder holds the ROM sets")
	target_compile_definitions(core_tests PRIVATE
		PINESY_TEST_RESOURCES="${PINESY_TEST_RESOURCES}"
		PINESY_TEST_DATA="${CMAKE_CURRENT_BINARY_DIR}/testdata")

	# Every engine run has a time limit, also when CTest's own fails.
	find_program(PINESY_TIMEOUT NAMES timeout gtimeout)
	set(_pinesy_limit)
	if(PINESY_TIMEOUT)
		set(_pinesy_limit "${PINESY_TIMEOUT}" -k 10 300)
	endif()
	add_test(NAME rules COMMAND ${_pinesy_limit} $<TARGET_FILE:core_tests> rules)
	add_test(NAME headless COMMAND ${_pinesy_limit} $<TARGET_FILE:core_tests> headless)
	set_tests_properties(rules headless PROPERTIES TIMEOUT 320)
endif()
