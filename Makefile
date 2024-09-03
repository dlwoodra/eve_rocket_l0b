#
# makefile
#

gpp = g++

INCLUDE_PATH = -I/usr/local/include

STD = -std=c++11

FAST_FLAGS = -O3 -funroll-loops -Wall -c
TEST_FLAGS = -Wall -c
DEBUG_FLAGS = -g -funroll-loops -Wall -c

#spdlog installs with fmt
LFLAGS = -lcfitsio -lm -lspdlog -lfmt -lokFrontPanel #to link statically add -static but the cfitsio needs curl, zcompress, maybe more 
LINKED_LIBS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib 
# the -Wl,-rpath= tells the linker to look in /usr/local/lib before /usr/lib
# without this g++ links with no errors but throws a runtime exception

#
# List the C source files that need to be compiled
#
COMSRC = CCSDSReader.cpp RecordFileWriter.cpp USBInputSource.cpp \
	FileInputSource.cpp fileutils.cpp FITSWriter.cpp PacketProcessor.cpp \
	TimeInfo.cpp assemble_image.cpp tai_to_ydhms.cpp LogFileWriter.cpp

SRCS = ${COMSRC} main.cpp
TEST_SRCS = ${COMSRC} test_main.cpp 

#
# Create a list of object files from the source files
#ld 
OBJS = ${SRCS:.c=.o}
TEST_OBJS = ${TEST_SRCS:.c=.o}

ROOT := .

cleanall: clean cleanrtlm removebinaries all

cleanql: clean cleanrtlm removebinaries ql

all: ql_test ql ql_debug

# for running tests
ql_test:
	rm -f *.o
	${gpp} ${STD} ${INCLUDE_PATH} ${TEST_FLAGS} ${TEST_SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${TEST_OBJS} ${LFLAGS}
	rm -f *.o

# for production use at SURF
ql:
	rm -f *.o
	${gpp} ${STD} ${INCLUDE_PATH} ${FAST_FLAGS} ${SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
	rm -f *.o

# for gdb
ql_debug:
	rm -f *.o
	${gpp} ${STD} ${INCLUDE_PATH} ${DEBUG_FLAGS} ${SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
	rm -f *.o

clean:
	rm -f *.o

cleanrtlm:
	find . -name "record*.rtlm" -size 0 -delete

removebinaries:
	rm -f ql ql_test ql_debug

.PHONY: all clean
