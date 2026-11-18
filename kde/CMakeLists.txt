# Pinesy for KDE Plasma: a Kirigami front end on libpinesy.

cmake_minimum_required(VERSION 3.20)
project(pinesy_kde VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Every package the app needs, each looked for on its own, so one
# configure lists every package that is missing, with the package of this
# system that has it. Each has its CMake name, its minimum version, and
# the name of its project.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PinesyPackages.cmake")
set(_missing "")
set(_old "")
foreach(_package IN ITEMS
		"ECM|6.0|extra-cmake-modules (KDE)"
		"Qt6Core|6.8|qtbase (Qt)"
		"Qt6Gui|6.8|qtbase (Qt)"
		"Qt6Widgets|6.8|qtbase (Qt)"
		"Qt6Qml|6.8|qtdeclarative (Qt)"
		"Qt6Quick|6.8|qtdeclarative (Qt)"
		"Qt6QuickControls2|6.8|qtdeclarative (Qt)"
		"KF6Kirigami|6.8|Kirigami (KDE Frameworks)"
		"KF6Config|6.8|KConfig (KDE Frameworks)"
		"KF6CoreAddons|6.8|KCoreAddons (KDE Frameworks)"
		"KF6I18n|6.8|KI18n (KDE Frameworks)")
	string(REPLACE "|" ";" _package "${_package}")
	list(GET _package 0 _name)
	list(GET _package 1 _version)
	list(GET _package 2 _project)
	find_package(${_name} QUIET CONFIG)
	if(NOT ${_name}_FOUND)
		list(APPEND _missing "${_project}|${_name}Config.cmake|${_version}")
	elseif(${_name}_VERSION VERSION_LESS _version)
		list(APPEND _old "${_project}: ${${_name}_VERSION} is installed, ${_version} or later is needed")
	endif()
endforeach()
if(_missing OR _old)
	list(REMOVE_DUPLICATES _old)
	set(PINESY_TOO_OLD ${_old})
	pinesy_missing_message(_message "KDE app" ${_missing})
	message(FATAL_ERROR "${_message}")
endif()

# The core, with the engine it links. It is added before the KDE compiler
# settings, which turn off C++ exceptions, so it builds with its own flags.
set(PINESY_CORE_TESTS OFF CACHE BOOL "Build the core's tests")
# The engine with MAME's Qt debugger, built by ../build-mame.sh --qt.
set(PINESY_ENGINE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../mame/libmame_pinesy_qt.so" CACHE FILEPATH "The engine library, libmame_pinesy_qt")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../core" core)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
# the executable is <build-dir>/bin/pinesy-kde
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
find_package(KF6 6.8 REQUIRED COMPONENTS Kirigami Config CoreAddons I18n)
qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

# The resources folder the core reads: the bundled ROM sets in "roms" and
# MAME's search folders. For a run from the build folder it is made here,
# with links to ../roms and to MAME's own folders, as ../app/build.sh
# copies them into the macOS bundle.
get_filename_component(PINESY_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(PINESY_RESOURCES_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/resources" CACHE PATH
	"The resources folder: ROM sets in its roms folder, and MAME's hash, plugins and other search folders")
if(PINESY_RESOURCES_FOLDER STREQUAL "${CMAKE_CURRENT_BINARY_DIR}/resources")
	file(MAKE_DIRECTORY "${PINESY_RESOURCES_FOLDER}")
	set(_pinesy_links
		"roms=${PINESY_PUBLIC_DIR}/roms"
		"plugins=${PINESY_PUBLIC_DIR}/mame/plugins"
		"hash=${PINESY_PUBLIC_DIR}/mame/hash"
		"bgfx=${PINESY_PUBLIC_DIR}/mame/bgfx"
		"artwork=${PINESY_PUBLIC_DIR}/mame/artwork"
		"language=${PINESY_PUBLIC_DIR}/mame/language")
	foreach(_link IN LISTS _pinesy_links)
		string(REPLACE "=" ";" _pair "${_link}")
		list(GET _pair 0 _name)
		list(GET _pair 1 _target)
		if(EXISTS "${_target}" AND NOT EXISTS "${PINESY_RESOURCES_FOLDER}/${_name}")
			file(CREATE_LINK "${_target}" "${PINESY_RESOURCES_FOLDER}/${_name}" SYMBOLIC)
		endif()
	endforeach()
endif()

add_executable(pinesy-kde
	src/main.cpp
	src/core.cpp
	src/core.h
	src/document.cpp
	src/document.h
	src/headless.cpp
	src/headless.h
	src/machineview.cpp
	src/machineview.h
	src/runner.cpp
	src/runner.h
	src/runwindow.cpp
	src/runwindow.h)

qt_add_qml_module(pinesy-kde
	URI Pinesy
	VERSION 1.0
	QML_FILES
		qml/Main.qml
		qml/DocumentWindow.qml
		qml/ConfigurationPage.qml
		qml/MachinePicker.qml
		qml/SettingsWindow.qml
		qml/GeneralSettingsPage.qml
		qml/DebuggerSettingsPage.qml)

target_compile_definitions(pinesy-kde PRIVATE
	PINESY_RESOURCES_FOLDER="${PINESY_RESOURCES_FOLDER}"
	PINESY_VERSION="${PROJECT_VERSION}")
target_include_directories(pinesy-kde PRIVATE src)
target_link_libraries(pinesy-kde PRIVATE
	pinesy
	Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets
	KF6::ConfigCore KF6::CoreAddons KF6::I18n KF6::I18nQml)

install(TARGETS pinesy-kde ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES com.stalepixels.pinesy.kde.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES com.stalepixels.pinesy.kde.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
# the app icon, from ../icons, under the app id
foreach(_size 16 24 32 48 64 128 256 512)
	install(FILES "${PINESY_PUBLIC_DIR}/icons/hicolor/${_size}x${_size}/apps/pinesy.png"
		DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/${_size}x${_size}/apps"
		RENAME com.stalepixels.pinesy.kde.png)
endforeach()
