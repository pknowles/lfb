#My all-in-one makefile. By Pyarelal Knowles.

#make commands:
#	<default> - debug
#	debug - for gdb
#	prof - for gprof
#	opt - optimizations
#	clean - remove intermediates
#	cleaner - clean + recurse into DEP_LIBS
#	echodeps - print DEP_LIBS and child DEP_LIBS

#TODO: automatically include LIBRARIES specified by child makefiles

#change these. TARGET with .a and .so are handled automatically. point DEP_LIBS to dependent libraries for recursive compilation
TARGET=liblfb$(ASFX).a
DEP_LIBS=../pyarlib/pyarlib$(ASFX).a
CFLAGS= -Wno-unused-parameter -Wno-unused-variable `pkg-config freetype2 --cflags` -std=c++11 -Wall -Wextra -D_GNU_SOURCE -Wfatal-errors
LIBRARIES=
CC=g++
SOURCE_SEARCH=
EXCLUDE_SOURCE=
PRECOMPILED_HEADER=
TMP=.build

include ../pyarlib/recursive.make

#shader embed dependencies
$(TMP)/lfbBase.o: shaders/lfb.glsl shaders/lfbZero.vert shaders/lfbTiles.glsl shaders/lfbSort.vert shaders/sorting.glsl
$(TMP)/lfbL.o: shaders/lfbL.glsl shaders/lfbRaggedSort.vert shaders/lfbCopy.vert shaders/prefixSums.vert
$(TMP)/lfbCL.o: shaders/lfbCL.glsl shaders/lfbRaggedSort.vert shaders/lfbCopy.vert shaders/prefixSums.vert shaders/prefixSumsM.vert shaders/lfbCLAlign.vert
$(TMP)/lfbLL.o: shaders/lfbLL.glsl
$(TMP)/lfbB.o: shaders/lfbB.glsl
$(TMP)/lfbPLL.o: shaders/lfbPLL.glsl

$(TMP)/lfbBase.o: export CCACHE_DISABLE=1
$(TMP)/lfbL.o: export CCACHE_DISABLE=1
$(TMP)/lfbCL.o: export CCACHE_DISABLE=1
$(TMP)/lfbLL.o: export CCACHE_DISABLE=1
$(TMP)/lfbB.o: export CCACHE_DISABLE=1
$(TMP)/lfbPLL.o: export CCACHE_DISABLE=1

