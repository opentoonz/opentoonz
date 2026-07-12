#!/bin/sh

#
# Test suite for kissfft
#
# Copyright (c) 2021, Vasyl Gello.
# This file is part of KISS FFT - https://github.com/mborgerding/kissfft
#
# SPDX-License-Identifier: BSD-3-Clause
# See COPYING file for more information.
#

if [ ! -f CHANGELOG ] && [ ! -f kiss_fft.h ]; then
	echo "ERROR: Please run this testsuite from top level of kissfft source tree!" >&2
	return 1
fi

TESTSUITEOUTDIR="$2"

if [ -z "$TESTSUITEOUTDIR" ]; then
	TESTSUITEOUTDIR="/tmp/kissfft-testsuite"
fi

if ! mkdir -p "$TESTSUITEOUTDIR"; then
	echo "ERROR: Can not create directory '$TESTSUITEOUTDIR'!" >&2
	return 1
fi

#
# Test runner function
#
# Parameters:
#
# $1 - Action:            "test" or "install"
# $2 - Build type:        "make" or "cmake"
# $3 - Data type:         "float" "double" "int16_t" "int32_t" "simd"
# $4 - library type:      "shared"  or "static"
# $5 - Include tools:     "yes" or "no"
# $6 - Install root dir:  "existing writable directory"
#

test_runner() {
	_ACTION="$1"
	_BUILD_TYPE="$2"
	_DATA_TYPE="$3"
	_LIB_TYPE="$4"
	_OPENMP="$5"
	_INCLUDE_TOOLS="$6"
	_INSTALL_ROOT_DIR="$7"

	_CMAKE_OPTS=""
	_MAKE_OPTS=""

	# Prepare install directory name without "$_OPENMP" and "$_INCLUDE_TOOLS"

	_INSTALL_DIR="$_INSTALL_ROOT_DIR/$_BUILD_TYPE/$_DATA_TYPE/$_LIB_TYPE"

	# Prepare log file without "$_OPENMP" and "$_INCLUDE_TOOLS"

	_LOG_FILE="$_INSTALL_ROOT_DIR/$_ACTION-$_BUILD_TYPE-$_DATA_TYPE-$_LIB_TYPE"

	# Validate parameters

	# Create install root directory

	if [ -z "$_INSTALL_ROOT_DIR" ]; then
		echo "" >&2
		echo "ERROR: Empty path to writeable directory" >&2
		echo "" >&2
		return 1
	fi

	if [ ! -d "$_INSTALL_ROOT_DIR" ]; then
		if ! mkdir -p "$_INSTALL_ROOT_DIR"; then
			echo "" >&2
			echo "ERROR: Can not create directory '$_INSTALL_ROOT_DIR'" >&2
			echo "" >&2
			return 1
		fi
	fi

	if [ "$_BUILD_TYPE" != "make" ] && [ "$_BUILD_TYPE" != "cmake" ]; then
		echo "ERROR: Build type must be one of: cmake make" >&2
		echo "" >&2
		return 1
	fi

	if [ "$_DATA_TYPE" != "double" ] &&
		[ "$_DATA_TYPE" != "float" ] &&
		[ "$_DATA_TYPE" != "int16_t" ] &&
		[ "$_DATA_TYPE" != "int32_t" ] &&
		[ "$_DATA_TYPE" != "simd" ];
	then
		echo "ERROR: Data type must be one of: double float int16_t int32_t simd" >&2
		echo "" >&2
		return 1
	else
		_MAKE_OPTS="$_MAKE_OPTS KISSFFT_DATATYPE=$_DATA_TYPE"
		_CMAKE_OPTS="$_CMAKE_OPTS -DKISSFFT_DATATYPE=$_DATA_TYPE"
	fi

	if [ "$_LIB_TYPE" != "shared" ] && [ "$_LIB_TYPE" != "static" ]; then
		echo "ERROR: Library type must be one of: shared static" >&2
		echo "" >&2
		return 1
	fi

	case "$_LIB_TYPE" in
		"shared")
			;;
		"static")
			_MAKE_OPTS="$_MAKE_OPTS KISSFFT_STATIC=1"
			_CMAKE_OPTS="$_CMAKE_OPTS -DKISSFFT_STATIC=ON"
			;;
		"*")
			echo "ERROR: OpenMP inclusion must be one of: no yes" >&2
			echo "" >&2
			return 1
			;;
	esac

	case "$_OPENMP" in
		"yes")
			_INSTALL_DIR="$_INSTALL_DIR/openmp"
			_LOG_FILE="$_LOG_FILE-openmp"
			_MAKE_OPTS="$_MAKE_OPTS KISSFFT_OPENMP=1"
			_CMAKE_OPTS="$_CMAKE_OPTS -DKISSFFT_OPENMP=ON"
			;;
		"no")
			_INSTALL_DIR="$_INSTALL_DIR/noopenmp"
			_LOG_FILE="$_LOG_FILE-noopenmp"
			;;
		"*")
			echo "ERROR: OpenMP inclusion must be one of: no yes" >&2
			echo "" >&2
			return 1
			;;
	esac

	case "$_INCLUDE_TOOLS" in
		"yes")
			_INSTALL_DIR="$_INSTALL_DIR/tools"
			_LOG_FILE="$_LOG_FILE-tools"
			;;
		"no")
			_INSTALL_DIR="$_INSTALL_DIR/notools"
			_LOG_FILE="$_LOG_FILE-notools"
			_MAKE_OPTS="$_MAKE_OPTS KISSFFT_TOOLS=0"
			_CMAKE_OPTS="$_CMAKE_OPTS -DKISSFFT_TOOLS=OFF"
			;;
		"*")
			echo "ERROR: Tools inclusion must be one of: no yes" >&2
			echo "" >&2
			return 1
			;;
	esac

	# Clean kissfft

	rm -rf build 1>/dev/null 2>/dev/null
	make clean 1>/dev/null 2>&1

	# Prepare status line

	_STATUS_LINE="Running: $(printf "% 10s" "$_ACTION") |"
	_STATUS_LINE="$_STATUS_LINE Build Type: $(printf "% 7s" "$_BUILD_TYPE") |"
	_STATUS_LINE="$_STATUS_LINE Data Type: $(printf "% 7s" "$_DATA_TYPE") |"
	_STATUS_LINE="$_STATUS_LINE Lib Type: $(printf "% 7s" "$_LIB_TYPE") |"
	_STATUS_LINE="$_STATUS_LINE OpenMP: $(printf "% 3s" "$_OPENMP") |"
	_STATUS_LINE="$_STATUS_LINE Tools: $(printf "% 3s" "$_INCLUDE_TOOLS") |"

	# Skip tests with tools not installed as they are same as with tools

	if [ "$_ACTION" = "test" ] && [ "$_INCLUDE_TOOLS" = "no" ]; then
		return 2
	fi

	# Run selected action

	echo "$_STATUS_LINE"

	case "$_ACTION" in
		"test")
			_MAKE_OPTS="$_MAKE_OPTS PREFIX=$_INSTALL_DIR"
			_CMAKE_OPTS="$_CMAKE_OPTS -DCMAKE_INSTALL_PREFIX=$_INSTALL_DIR"

			case "$_BUILD_TYPE" in
				"make")
					make $_MAKE_OPTS all 1>>"$_LOG_FILE" 2>&1 &&
						make $_MAKE_OPTS testsingle 1>>"$_LOG_FILE" 2>&1 &&
					_RET=$?
					;;
				"cmake")
					mkdir build 1>/dev/null 2>&1 &&
						cd build &&
						cmake $_CMAKE_OPTS .. 1>"$_LOG_FILE" 2>&1 &&
					make all 1>>"$_LOG_FILE" 2>&1 &&
					make test 1>>"$_LOG_FILE" 2>&1
					_RET=$?
					cd ..
					;;
			esac
			;;
		"install")
			_MAKE_OPTS="$_MAKE_OPTS PREFIX=$_INSTALL_DIR"
			_CMAKE_OPTS="$_CMAKE_OPTS -DCMAKE_INSTALL_PREFIX=$_INSTALL_DIR"

			case "$_BUILD_TYPE" in
				"make")
					make $_MAKE_OPTS install 1>>"$_LOG_FILE" 2>&1
					_RET=$?
					;;
				"cmake")
					mkdir build 1>/dev/null 2>&1 &&
						cd build &&
						cmake $_CMAKE_OPTS .. 1>"$_LOG_FILE" 2>&1 &&
						make all 1>>"$_LOG_FILE" 2>&1 &&
						make install 1>>"$_LOG_FILE" 2>&1
					_RET=$?
					cd ..
					;;
			esac
			;;
		*)
			echo "ERROR: Action must be one of: test install" >&2
			echo "" >&2
			return 1
			;;
	esac

	# Clean kissfft

	rm -rf build 1>/dev/null 2>/dev/null
	make clean 1>/dev/null 2>&1

	# Return result

	[ $_RET -eq 0 ] && return 0 || return 1
}

# Main script

for ACTION in test install; do
	for BUILD_TYPE in make cmake; do
		for DATA_TYPE in float double int16_t int32_t simd; do
			for LIB_TYPE in shared static; do
				for OPENMP in no yes; do
					for INCLUDE_TOOLS in no yes; do
						test_runner \
							"$ACTION" \
							"$BUILD_TYPE" \
							"$DATA_TYPE" \
							"$LIB_TYPE" \
							"$OPENMP" \
							"$INCLUDE_TOOLS" \
							"$TESTSUITEOUTDIR"

						case $? in
						0)
							echo "Result: OK"
							;;
						1)
							echo "Result: FAIL"
							;;
						2)
							# Ignore it
							echo "Result: IGNORE" 1>/dev/null
							;;
						esac
					done
				done
			done
		done
	done
done 2>&1 | tee "$TESTSUITEOUTDIR/all-tests.log"
