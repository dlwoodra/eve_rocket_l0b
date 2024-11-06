#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "LogFileWriter.hpp"

class FileCompressor {
public:
    // Public method to start the compression in a separate thread
    void compressFile(const std::string& inputFilename);

private:
    // Method to call pigz to compress a file
    void compressWithPigz(const std::string& inputFile);

    // could add more compression methods here besides pigz, maybe pbzip2, etc.

    // Method to compress a file and time the operation
    void compressAndTime(const std::string& inputFilename);
};

#endif // COMPRESSOR_HPP
