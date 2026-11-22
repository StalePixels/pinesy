# pinesy_gtk: the GNOME front end of Pi/Ne-Sy (GTK4, libadwaita, gtkmm).

cmake_minimum_required(VERSION 3.20)
project(pinesy_gtk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Every package the app needs, each looked for first, so one configure
# lists every package that is missing, with the package of this system
# that has it.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PinesyPackages.cmake")
set(_missing "")
set(_old "")
foreach(_package IN ITEMS "gtkmm-4.0|4.10|gtkmm" "libadwaita-1|1.6|libadwaita" "libsoup-3.0|3.0|libsoup")
	string(REPLACE "|" ";" _package "${_package}")
	list(GET _package 0 _module)
	list(GET _package 1 _version)
	list(GET _package 2 _project)
	pinesy_pc_version(${_module} _installed)
	if(NOT _installed)
		list(APPEND _missing "${_project}|${_module}.pc|${_version}")
	elseif(_installed VERSION_LESS _version)
		list(APPEND _old "${_project}: ${_installed} is installed, ${_version} or later is needed")
	endif()
endforeach()
foreach(_program IN ITEMS pkg-config glib-compile-schemas)
	find_program(_found_${_program} ${_program})
	if(NOT _found_${_program})
		list(APPEND _missing "${_program}|/usr/bin/${_program}|0")
	endif()
endforeach()
if(_missing OR _old)
	set(PINESY_TOO_OLD ${_old})
	pinesy_missing_message(_message "GNOME app" ${_missing})
	message(FATAL_ERROR "${_message}")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.10)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(SOUP REQUIRED IMPORTED_TARGET libsoup-3.0>=3.0)
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED)

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
	src/recent_window.cpp
	src/settings.cpp
	src/settings_dialog.cpp
	src/signals.cpp
	src/updates.cpp)
target_compile_definitions(pinesy_gtk PRIVATE
	PINESY_APP_ID="${PINESY_APP_ID}"
	PINESY_RESOURCES_DIR="${PINESY_RESOURCES_DIR}")
set_target_properties(pinesy_gtk PROPERTIES OUTPUT_NAME pinesy)
target_link_libraries(pinesy_gtk PRIVATE pinesy PkgConfig::GTKMM PkgConfig::ADWAITA PkgConfig::SOUP)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(pinesy_gtk PRIVATE -Wall -Wextra)
endif()
add_dependencies(pinesy_gtk pinesy_schemas)

# Install: the app in bin, libpinesy and the engine in lib, the resources
# folder in share/pinesy/resources, and the files the desktop reads. An
# installed copy finds its resources folder and its settings schema
# beside itself (src/settings.cpp).
include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PinesyInstall.cmake")
pinesy_install(pinesy_gtk
	BINDIR "${CMAKE_INSTALL_BINDIR}"
	LIBDIR "${CMAKE_INSTALL_LIBDIR}"
	DATADIR "${CMAKE_INSTALL_DATADIR}")
file(RELATIVE_PATH _pinesy_installed_schemas "/${CMAKE_INSTALL_BINDIR}" "/${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas")
target_compile_definitions(pinesy_gtk PRIVATE PINESY_INSTALLED_SCHEMAS="${_pinesy_installed_schemas}")
install(TARGETS pinesy_gtk RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# The settings schema, compiled where it is installed
install(FILES "data/${_pinesy_schema}" DESTINATION "${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas")
# A Debian package leaves the compiling to the system (PinesyDeb.cmake).
option(PINESY_INSTALL_COMPILED_SCHEMAS "Compile the installed settings schemas" ON)
if(PINESY_INSTALL_COMPILED_SCHEMAS)
	install(CODE "execute_process(
		COMMAND \"${GLIB_COMPILE_SCHEMAS}\" --strict \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas\"
		COMMAND_ERROR_IS_FATAL ANY)")
endif()

install(FILES data/${PINESY_APP_ID}.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES data/${PINESY_APP_ID}.mime.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
	RENAME ${PINESY_APP_ID}.xml)
# D-Bus starts the app for the desktop (DBusActivatable in the .desktop
# file). The service file names the program by its full path under
# CMAKE_INSTALL_PREFIX as it was when CMake configured the build.
configure_file(data/${PINESY_APP_ID}.service.in ${PINESY_APP_ID}.service @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PINESY_APP_ID}.service" DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services")
# the app icon, from ../icons, under the app id
foreach(_size 16 24 32 48 64 128 256 512)
	install(FILES "${_pinesy_public}/icons/hicolor/${_size}x${_size}/apps/pinesy.png"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps"
		RENAME ${PINESY_APP_ID}.png)
endforeach()

# The Debian package (../cmake/PinesyDeb.cmake): cpack -G DEB
include("${_pinesy_public}/cmake/PinesyDeb.cmake")
string(REGEX REPLACE "^/" "" _pinesy_prefix "${CMAKE_INSTALL_PREFIX}")
pinesy_deb(NAME pinesy SUMMARY "Pi/Ne-Sy, a Pi/Next-System emulator: the GNOME app"
	LIBDIR "${_pinesy_prefix}/${CMAKE_INSTALL_LIBDIR}" DEPENDS pinesy-gdb)
include(CPack)
