#
# makefile
#

# note that this relies on precompiling spdlog_pch.hpp
# use make spdlog_pch

#openmp can be enabled with CXXFLAGS and LFLAGS, disable for now

CXX = g++
INCLUDE_PATH = -I/usr/local/include 
CXXFLAGS = -std=c++11 #-fopenmp
PCH_FLAGS = -include spdlog_pch.hpp

FAST_FLAGS = -O3 -funroll-loops -Wall
TEST_FLAGS = -Wall
DEBUG_FLAGS = -g -funroll-loops -Wall

LFLAGS = -lcfitsio -lm -lspdlog -lfmt -lokFrontPanel #-fopenmp
LINKED_LIBS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib 

COMSRC = CCSDSReader.cpp RecordFileWriter.cpp USBInputSource.cpp \
    FileInputSource.cpp FITSWriter.cpp PacketProcessor.cpp \
    TimeInfo.cpp assemble_image.cpp tai_to_ydhms.cpp LogFileWriter.cpp \
    commonFunctions.cpp

SRCS = ${COMSRC} main.cpp
TEST_SRCS = ${COMSRC} test_main.cpp 

OBJS = ${SRCS:.cpp=.o}
TEST_OBJS = ${TEST_SRCS:.cpp=.o}

# actions, not files
.PHONY: all clean cleanrtlm cleanlog removebinaries

all: ql_test ql ql_debug

# Target to build the precompiled header
spdlog_pch: spdlog_pch.pch

# Rule to generate the precompiled header
spdlog_pch.pch: spdlog_pch.hpp
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -x c++-header spdlog_pch.hpp -o spdlog_pch.pch

# for running tests
ql_test: spdlog_pch
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(TEST_FLAGS) $(PCH_FLAGS) -c $(TEST_SRCS)
	$(CXX) $(LINKED_LIBS) -o $@ $(TEST_OBJS) $(LFLAGS)
	rm -f $(TEST_OBJS)

# for production use at SURF
ql: spdlog_pch
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(FAST_FLAGS) $(PCH_FLAGS) -c $(SRCS)
	$(CXX) $(LINKED_LIBS) -o $@ $(OBJS) $(LFLAGS)
	rm -f $(OBJS)

# for gdb
ql_debug: spdlog_pch
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(DEBUG_FLAGS) $(PCH_FLAGS) -c $(SRCS)
	$(CXX) $(LINKED_LIBS) -o $@ $(OBJS) $(LFLAGS)
	rm -f $(OBJS)

clean:
	rm -f $(OBJS) $(TEST_OBJS)

cleanrtlm:
	find . -name "record*.rtlm" -size 0 -delete

cleanlog:
	find . -name "log*.log" -size 0 -delete

removebinaries:
	rm -f ql ql_test ql_debug



# CXX = g++

# INCLUDE_PATH = -I/usr/local/include 

# CXXFLAGS = -std=c++11

# FAST_FLAGS = -O3 -funroll-loops -include spdlog_pch.hpp -Wall -c
# TEST_FLAGS = -include spdlog_pch.hpp -Wall -c
# DEBUG_FLAGS = -g -funroll-loops -include spdlog_pch.hpp -Wall -c

# #spdlog installs with fmt
# LFLAGS = -lcfitsio -lm -lspdlog -lfmt -lokFrontPanel #to link statically add -static but the cfitsio needs curl, zcompress, maybe more 
# LINKED_LIBS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib 
# # the -Wl,-rpath= tells the linker to look in /usr/local/lib before /usr/lib
# # without this g++ links with no errors but throws a runtime exception

# #
# # List the C source files that need to be compiled
# #
# COMSRC = CCSDSReader.cpp RecordFileWriter.cpp USBInputSource.cpp \
# 	FileInputSource.cpp FITSWriter.cpp PacketProcessor.cpp \
# 	TimeInfo.cpp assemble_image.cpp tai_to_ydhms.cpp LogFileWriter.cpp \
# 	commonFunctions.cpp

# SRCS = ${COMSRC} main.cpp
# TEST_SRCS = ${COMSRC} test_main.cpp 

# #
# # Create a list of object files from the source files
# #ld 
# OBJS = ${SRCS:.c=.o}
# TEST_OBJS = ${TEST_SRCS:.c=.o}

# ROOT := .

# cleanall: clean cleanrtlm cleanlog removebinaries all

# cleanql: clean cleanrtlm cleanlog removebinaries ql

# all: ql_test ql ql_debug

# # Target to build the precompield header
# spdlog_pch: spdlog_pch.pch

# # Rule to generate the precompiled header
# spdlog_pch.pch: spdlog_pch.hpp
# 	${CXX} ${CXXFLAGS} -x c++-header spdlog_pch.hpp -o spdlog_pch.pch

# # for running tests
# ql_test:
# 	rm -f *.o
# 	${CXX} ${CXXFLAGS} ${INCLUDE_PATH} ${TEST_FLAGS} ${TEST_SRCS}
# 	${CXX} ${LINKED_LIBS} -o $@ ${TEST_OBJS} ${LFLAGS}
# 	rm -f *.o

# # for production use at SURF
# ql:
# 	rm -f *.o
# 	${CXX} ${CXXFLAGS} ${INCLUDE_PATH} ${FAST_FLAGS} ${SRCS}
# 	${CXX} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
# 	rm -f *.o

# # for gdb
# ql_debug:
# 	rm -f *.o
# 	${CXX} ${CXXFLAGS} ${INCLUDE_PATH} ${DEBUG_FLAGS} ${SRCS}
# 	${CXX} ${LINKED_LIBS} -o $@ ${OBJS} ${LFLAGS}
# 	rm -f *.o

# clean:
# 	rm -f *.o

# cleanrtlm:
# 	find . -name "record*.rtlm" -size 0 -delete

# cleanlog:
# 	find . -name "log*.log" -size 0 -delete

# removebinaries:
# 	rm -f ql ql_test ql_debug

# .PHONY: all clean
