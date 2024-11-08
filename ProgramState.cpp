// ProgramState.cpp
#include "ProgramState.hpp"

// global variables to provide the program state
ProgramState globalState;
std::mutex mtx;

void globalStateInit() {
    std::cout << "globablStateInit executing" << std::endl;
    LogFileWriter::getInstance().logInfo("globalStateInit executing");

    mtx.lock();
    globalState.megsa.image[0][0] = {0xff};
    globalState.megsb.image[0][0] = {0x3fff};
#ifdef ENABLEGUI
    globalState.guiEnabled = true;
#endif
    globalState.running = true;
    globalState.initComplete = true;
    mtx.unlock(); // unlock the mutex
}