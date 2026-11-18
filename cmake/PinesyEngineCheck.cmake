# Checks, before the engine build starts, that this Linux system has what
# build-mame.sh needs. Run as a script:
#
#   cmake -DFLAVOUR=<plain|gtk|qt> -P PinesyEngineCheck.cmake
#
# It prints nothing when everything is there. Otherwise it prints what to
# install, what is too old on this OS and what this OS does not have.
# Without this check, a missing clang shows up as a MAME error about the
# GCC version, because MAME cannot read the version of a compiler that
# is not there.

cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/PinesyPackages.cmake")

set(_missing "")

# programs: what they are, and the path the packages install them at
foreach(_program IN ITEMS clang clang++ ccache make python3 ninja pkg-config git)
	find_program(_found_${_program} ${_program})
	if(NOT _found_${_program})
		list(APPEND _missing "${_program}|/usr/bin/${_program}|0")
	endif()
endforeach()

# QEMU's build makes a Python virtual environment
find_program(_python python3)
if(_python)
	execute_process(COMMAND ${_python} -c "import venv, ensurepip"
		RESULT_VARIABLE _venv OUTPUT_QUIET ERROR_QUIET)
	if(NOT _venv EQUAL 0)
		execute_process(COMMAND ${_python} -c "import sysconfig; print(sysconfig.get_path('stdlib'))"
			OUTPUT_VARIABLE _stdlib OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
		list(APPEND _missing "Python's venv module|${_stdlib}/ensurepip/__init__.py|0")
	endif()
endif()

# libraries, by their pkg-config file, looked for without pkg-config so
# they are checked even when pkg-config itself is missing
set(_modules "GLib|glib-2.0" "ALSA|alsa" "PulseAudio|libpulse" "PipeWire|libpipewire-0.3")
if(FLAVOUR STREQUAL "gtk")
	list(APPEND _modules "GTK 4|gtk4")
elseif(FLAVOUR STREQUAL "qt")
	list(APPEND _modules "Qt 6 Widgets|Qt6Widgets")
endif()
foreach(_module IN LISTS _modules)
	string(REPLACE "|" ";" _module "${_module}")
	list(GET _module 0 _what)
	list(GET _module 1 _name)
	pinesy_pc_version(${_name} _pc)
	if(NOT _pc)
		list(APPEND _missing "${_what} development files|${_name}.pc|0")
	endif()
endforeach()

if(FLAVOUR STREQUAL "qt")
	find_program(_qmake6 qmake6)
	if(NOT _qmake6 AND NOT QT_HOME)
		list(APPEND _missing "qmake6|/usr/bin/qmake6|0")
	endif()
endif()

if(_missing)
	pinesy_missing_message(_message "engine" ${_missing})
	message(NOTICE "${_message}")
endif()
