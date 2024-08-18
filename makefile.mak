#
# makefile
#

gpp = g++

#INCLUDE_PATH = -I/home/dlwoodra/rocket/eve_rocket_l0b/cfitsio/include/include
INCLUDE_PATH = -I/usr/local/include

#FAST_FLAGS = -I${eve_code_include} -O3 -funroll-loops -Wall -c -fmessage-length=0
#DEBUG_FLAGS = -I${eve_code_include} -O0 -g3 -funroll-loops -Wall -c -fmessage-length=0
FAST_FLAGS = -O3 -funroll-loops -Wall -c
DEBUG_FLAGS = -O0 -g -funroll-loops -Wall -c

#LFLAGS = -leve_utils -llogutilities 
#LINKED_LIBS = -L${eve_code_lib}
LFLAGS = -lcfitsio -lm  #to link statically add -static but the cfitsio needs curl, zcompress, maybe more 
#LINKED_LIBS = -L/home/dlwoodra/rocket/eve_rocket_l0b/cfitsio/lib/lib -Wl,-rpath=/home/dlwoodra/rocket/eve_rocket_l0b/cfitsio/lib/lib
LINKED_LIBS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib
# the -Wl,-rpath= tells the linker to look in /usr/local/lib before /usr/lib
# without this g++ links with no errors but throws a runtime exception

#
# List the C source files that need to be compiled
#
SRCS = CCSDSReader.cpp RecordFileWriter.cpp fileutils.cpp FITSWriter.cpp PacketProcessor.cpp main.cpp 
TEST_SRCS = CCSDSReader.cpp RecordFileWriter.cpp fileutils.cpp FITSWriter.cpp PacketProcessor.cpp test_main.cpp 

#
# Create a list of object files from the source files
#ld 
OBJS = ${SRCS:.c=.o}
TEST_OBJS = ${TEST_SRCS:.c=.o}

ROOT := .

cleanall: clean removebinaries all

all: test_ql ql ql_debug

# for running tests
test_ql:
	rm -f *.o
	${gpp} -std=c++11 ${INCLUDE_PATH} ${FAST_FLAGS} ${TEST_SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${TEST_OBJS} ${LFLAGS}
	rm -f *.o

# for production use at SURF
ql:
	rm -f *.o
	${gpp} ${INCLUDE_PATH} ${FAST_FLAGS} ${SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
	rm -f *.o

# for gdb
ql_debug:
	rm -f *.o
	${gpp} ${INCLUDE_PATH} ${DEBUG_FLAGS} ${SRCS}
	${gpp} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
	rm -f *.o

clean:
	rm -f *.o

removebinaries:
	rm -f ql test_ql ql_debug

.PHONY: all clean
