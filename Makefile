#
# makefile
#

# note that this relies on precompiling spdlog_pch.hpp
# use make spdlog_pch

#openmp is enabled with CXXFLAGS and LFLAGS

CXX = g++
INCLUDE_PATH = -I/usr/local/include 
CXXFLAGS = -std=c++11 -fopenmp

# Check if SKIPPARITY is defined during compilation
## active using make SKIPPARITY=1 target
ifdef SKIPPARITY
    CXXFLAGS += -DSKIPPARITY
endif

PCH_FLAGS = -include spdlog_pch.hpp

FAST_FLAGS = -O2 -Wall #-g
TEST_FLAGS = -Wall #-g
DEBUG_FLAGS = -g -Wall

LFLAGS = -lcfitsio -lm -lspdlog -lfmt -lokFrontPanel -fopenmp
LINKED_LIBS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib 

COMSRC = LogFileWriter.cpp CCSDSReader.cpp RecordFileWriter.cpp \
	USBInputSource.cpp FileInputSource.cpp FITSWriter.cpp PacketProcessor.cpp \
    TimeInfo.cpp assemble_image.cpp tai_to_ydhms.cpp \
    commonFunctions.cpp FileCompressor.cpp

SRCS = ${COMSRC} main.cpp
TEST_SRCS = ${COMSRC} test_main.cpp 

COM_OBJS = ${COMSRC:.cpp=.o}

MAIN_OBJS = main.o
TEST_OBJS = test_main.o

# Function to compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(FAST_FLAGS) -c $< -o $@

# actions, not files
.PHONY: all clean removebinaries

all: rl0b_test rl0b_main rl0b_main_debug

# Target to build the precompiled header
spdlog_pch: spdlog_pch.pch

# Rule to generate the precompiled header
spdlog_pch.pch: spdlog_pch.hpp
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -x c++-header spdlog_pch.hpp -o spdlog_pch.pch

# for running tests
rl0b_test: spdlog_pch $(COM_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(TEST_FLAGS) $(PCH_FLAGS) -o $@ $(COM_OBJS) $(TEST_OBJS) $(LINKED_LIBS) $(LFLAGS)
	rm -f $(TEST_OBJS)

# for production use at SURF
rl0b_main: spdlog_pch $(COM_OBJS) $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(FAST_FLAGS) $(PCH_FLAGS) -o $@ $(COM_OBJS) $(MAIN_OBJS) $(LINKED_LIBS) $(LFLAGS)

# for gdb
rl0b_main_debug: spdlog_pch $(COM_OBJS) $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(DEBUG_FLAGS) $(PCH_FLAGS) -o $@ $(COM_OBJS) $(MAIN_OBJS) $(LINKED_LIBS) $(LFLAGS)

clean:
	rm -f $(COM_OBJS) $(MAIN_OBJS) $(TEST_OBJS)
	find . -name "record*.rtlm" -size 0 -delete
	find . -name "log*.log" -size 0 -delete

removebinaries:
	rm -f rl0b_main rl0b_test rl0b_main_debug
