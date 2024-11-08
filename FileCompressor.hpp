#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include "LogFileWriter.hpp"
#include "ProgramState.hpp"

class FileCompressor {
public:
    // Public method to start the compression in a separate thread
    void compressFile(const std::string& inputFilename);
    // void waitForAllThreads();

private:
    // Method to call pigz to compress a file
    void compressWithPigz(const std::string& inputFile);

    // could add more compression methods here besides pigz, maybe pbzip2, etc.

    // Method to compress a file and time the operation
    void compressAndTime(const std::string& inputFilename);

    // // Store threads and mutex to protect the container
    // std::vector<std::thread> compressionThreads; // Store threads here
    // std::mutex compressorThreadMutex; // Mutex to protect the thread container
};


#endif // COMPRESSOR_HPP
