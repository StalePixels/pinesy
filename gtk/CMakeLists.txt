# pinesy_gtk: the GNOME front end of Pi/Ne-Sy (GTK4, libadwaita, gtkmm).

cmake_minimum_required(VERSION 3.20)
project(pinesy_gtk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Every package the app needs, by the name of its project. All of them are
# looked for first, so one configure lists every package that is missing.
set(_missing "")
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(GTKMM QUIET IMPORTED_TARGET gtkmm-4.0>=4.10)
	if(NOT GTKMM_FOUND)
		list(APPEND _missing "gtkmm 4.10 or later")
	endif()
	pkg_check_modules(ADWAITA QUIET IMPORTED_TARGET libadwaita-1>=1.6)
	if(NOT ADWAITA_FOUND)
		list(APPEND _missing "libadwaita 1.6 or later")
	endif()
else()
	list(APPEND _missing "pkg-config")
endif()
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
if(NOT GLIB_COMPILE_SCHEMAS)
	list(APPEND _missing "GLib's glib-compile-schemas tool")
endif()
if(_missing)
	list(JOIN _missing "\n  " _missing)
	message(FATAL_ERROR "Install these from your distribution, with their development files:\n  ${_missing}")
endif()

# The core, libpinesy, built from ../core. Its tests are not part of
# this build.
set(PINESY_CORE_TESTS OFF)
# The engine with the GTK debugger, built by ../build-mame.sh --gtk.
set(PINESY_ENGINE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../mame/libmame_pinesy_gtk.so" CACHE FILEPATH "The engine library, libmame_pinesy_gtk")
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
