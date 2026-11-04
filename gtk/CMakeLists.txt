# pinesy_gtk: the GNOME front end of Pi/Ne-Sy (GTK4, libadwaita, gtkmm).

cmake_minimum_required(VERSION 3.20)
project(pinesy_gtk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.10)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED)

# The core, libpinesy, built from ../core with the engine it finds
# there (../mame/libmame_pinesy.so unless PINESY_ENGINE_LIBRARY says
# otherwise). Its tests are not part of this build.
set(PINESY_CORE_TESTS OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../core" "${CMAKE_CURRENT_BINARY_DIR}/core")

set(PINESY_APP_ID "com.stalepixels.Pinesy")

# The resources folder the core reads: the bundled ROM sets in its
# "roms" folder and MAME's search folders. The default is a folder in
# the build tree that links to ../roms and to MAME's own folders.
set(PINESY_RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources" CACHE PATH "The resources folder the app gives the core")

get_filename_component(_pinesy_public "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(_pinesy_resources "${CMAKE_CURRENT_BINARY_DIR}/resources")
file(MAKE_DIRECTORY "${_pinesy_resources}")
file(CREATE_LINK "${_pinesy_public}/roms" "${_pinesy_resources}/roms" SYMBOLIC)
foreach(_folder plugins hash bgfx artwork language)
	file(CREATE_LINK "${_pinesy_public}/mame/${_folder}" "${_pinesy_resources}/${_folder}" SYMBOLIC)
endforeach()

# The settings schema, compiled into the build tree. A run from the
# build tree finds it with GSETTINGS_SCHEMA_DIR=<build>/schemas.
set(_pinesy_schema "${PINESY_APP_ID}.gschema.xml")
set(_pinesy_schema_dir "${CMAKE_CURRENT_BINARY_DIR}/schemas")
add_custom_command(
	OUTPUT "${_pinesy_schema_dir}/gschemas.compiled"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${_pinesy_schema_dir}"
	COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/data/${_pinesy_schema}" "${_pinesy_schema_dir}/"
	COMMAND "${GLIB_COMPILE_SCHEMAS}" --strict "${_pinesy_schema_dir}"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/${_pinesy_schema}"
	COMMENT "Compiling the settings schema")
add_custom_target(pinesy_schemas ALL DEPENDS "${_pinesy_schema_dir}/gschemas.compiled")

add_executable(pinesy_gtk
	src/application.cpp
	src/configuration_window.cpp
	src/machine_picker.cpp
	src/machine_runner.cpp
	src/main.cpp
	src/settings.cpp
	src/settings_dialog.cpp
	src/signals.cpp)
target_compile_definitions(pinesy_gtk PRIVATE
	PINESY_APP_ID="${PINESY_APP_ID}"
	PINESY_RESOURCES_DIR="${PINESY_RESOURCES_DIR}")
set_target_properties(pinesy_gtk PROPERTIES OUTPUT_NAME pinesy)
target_link_libraries(pinesy_gtk PRIVATE pinesy PkgConfig::GTKMM PkgConfig::ADWAITA)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(pinesy_gtk PRIVATE -Wall -Wextra)
endif()
add_dependencies(pinesy_gtk pinesy_schemas)
