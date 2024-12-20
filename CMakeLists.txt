cmake_minimum_required(VERSION 3.10)

# converted from Makefile_gui 12/23/24
# to use cmake, run the following commands:
# mkdir build
# cd build
# cmake ..
# choose one of these
# cmake --build . --target rl0b_main_gui
# cmake --build . --target rl0b_test
# cmake --build . --target rl0b_main_debug

project(RL0B_GUI_Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
set(IMGUI_DIR ./imgui)
set(IMGUI_BACKEND_DIR ${IMGUI_DIR}/backends)
include_directories(/usr/local/include ${IMGUI_DIR} ${IMGUI_BACKEND_DIR})

# Compiler flags
set(CMAKE_CXX_FLAGS "-fopenmp -DENABLEGUI")
if(SKIPPARITY)
    add_definitions(-DSKIPPARITY)
endif()

set(FAST_FLAGS "-Wall -O1 -g")
set(TEST_FLAGS "-Wall -g")
set(DEBUG_FLAGS "-g -Wall")

# Linker flags and libraries
set(LFLAGS "-lcfitsio -lm -lspdlog -lfmt -lokFrontPanel -fopenmp -lGL -lz")
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

link_directories(/usr/local/lib ${GLFW_LIBRARY_DIRS})
link_libraries(${GLFW_STATIC_LIBRARIES} ${GLFW_LIBRARIES} ${LFLAGS})

# Precompiled header
set(PCH_HEADER spdlog_pch.hpp)
set(PCH_COMPILED spdlog_pch.pch)
add_custom_command(
    OUTPUT ${PCH_COMPILED}
    COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -x c++-header ${PCH_HEADER} -o ${PCH_COMPILED}
    DEPENDS ${PCH_HEADER}
)

# Common source files
set(COM_SRC
    CCSDSReader.cpp
    RecordFileWriter.cpp
    USBInputSource.cpp
    FileInputSource.cpp
    FITSWriter.cpp
    PacketProcessor.cpp
    TimeInfo.cpp
    assemble_image.cpp
    tai_to_ydhms.cpp
    LogFileWriter.cpp
    commonFunctions.cpp
    FileCompressor.cpp
    ProgramState.cpp
    imgui_thread.cpp
)

# ImGui source files
set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ./implot.cpp
    ./implot_items.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp
)

# Executable sources
set(MAIN_SOURCES main.cpp)
set(TEST_SOURCES test_main.cpp)

# Targets
add_executable(rl0b_test ${PCH_COMPILED} ${COM_SRC} ${TEST_SOURCES} ${IMGUI_SRC})
set_target_properties(rl0b_test PROPERTIES COMPILE_FLAGS "${TEST_FLAGS}")

add_executable(rl0b_main_gui ${PCH_COMPILED} ${COM_SRC} ${MAIN_SOURCES} ${IMGUI_SRC})
set_target_properties(rl0b_main_gui PROPERTIES COMPILE_FLAGS "${FAST_FLAGS}")

add_executable(rl0b_main_debug ${PCH_COMPILED} ${COM_SRC} ${MAIN_SOURCES} ${IMGUI_SRC})
set_target_properties(rl0b_main_debug PROPERTIES COMPILE_FLAGS "${DEBUG_FLAGS}")

# Clean-up targets
add_custom_target(clean_custom ALL
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/*.o ${CMAKE_BINARY_DIR}/*.pch
    COMMAND find . -name "record*.rtlm" -size 0 -delete
    COMMAND find . -name "log*.log" -size 0 -delete
)

add_custom_target(removebinaries ALL
    COMMAND ${CMAKE_COMMAND} -E remove rl0b_main_gui rl0b_test rl0b_main_debug
)
