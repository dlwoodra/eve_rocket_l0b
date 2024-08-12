#
# makefile
#

gpp = g++

#FAST_FLAGS = -I${eve_code_include} -O3 -funroll-loops -Wall -c -fmessage-length=0
#DEBUG_FLAGS = -I${eve_code_include} -O0 -g3 -funroll-loops -Wall -c -fmessage-length=0
FAST_FLAGS = -O3 -funroll-loops -Wall -c
DEBUG_FLAGS = -O0 -g3 -funroll-loops -Wall -c

#LFLAGS = -leve_utils -llogutilities 
#LINKED_LIBS = -L${eve_code_lib}

#
# List the C source files that need to be compiled
#
SRCS = CCSDSReader.cpp RecordFileWriter.cpp fileutils.cpp main.cpp 
TEST_SRCS = CCSDSReader.cpp RecordFileWriter.cpp fileutils.cpp test_main.cpp 

#
# Create a list of object files from the source files
#
OBJS = ${SRCS:.c=.o}
TEST_OBJS = ${TEST_SRCS:.c=.o}

ROOT := .

all: test_ql ql ql_debug

test_ql: 
	${gpp} -std=c++11 ${FAST_FLAGS} ${TEST_SRCS}
	${gpp} -o $@ ${TEST_OBJS} ${LFLAGS}
	rm *.o

ql: 
	${gpp} $(FAST_FLAGS) $(SRCS)
	${gpp} $(LINKED_LIBS) -o $@ $(OBJS) $(LFLAGS)
	rm *.o

ql_debug: 
	${gpp} $(DEBUG_FLAGS) $(SRCS)
	${gpp} $(LINKED_LIBS) -o $@ $(OBJS) $(LFLAGS)
	rm *.o

clean:
	rm -f *.o test_ql ql ql_debug

.PHONY: all clean
