# Pinesy for KDE Plasma: a Kirigami front end on libpinesy.

cmake_minimum_required(VERSION 3.20)
project(pinesy_kde VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The core, with the engine it links. It is added before the KDE compiler
# settings, which turn off C++ exceptions, so it builds with its own flags.
set(PINESY_CORE_TESTS OFF CACHE BOOL "Build the core's tests")
# The engine with MAME's Qt debugger, built by ../build-mame.sh --qt.
set(PINESY_ENGINE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../mame/libmame_pinesy_qt.so" CACHE FILEPATH "The engine library, libmame_pinesy_qt")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../core" core)

# CMake calls the package "ECM"; distributions name it extra-cmake-modules
find_package(ECM 6.0 QUIET NO_MODULE)
if(NOT ECM_FOUND)
	message(FATAL_ERROR "extra-cmake-modules (ECM) 6.0 or later is not installed. Install the extra-cmake-modules package of your distribution.")
endif()
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# Distributions split Qt into its modules: Core, Gui and Widgets are in
# qtbase, Qml, Quick and QuickControls2 in qtdeclarative
find_package(Qt6 6.8 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
if(NOT Qt6_FOUND)
	message(FATAL_ERROR "Qt 6.8 or later is not installed with all the parts this app needs. Install the development packages of your distribution for the Qt modules qtbase (Core, Gui, Widgets) and qtdeclarative (Qml, Quick, QuickControls2).")
endif()
qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)
find_package(KF6 6.8 REQUIRED COMPONENTS Kirigami Config CoreAddons I18n)

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
