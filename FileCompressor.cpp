#include "FileCompressor.hpp"

// Public method to start the compression in a separate thread
void FileCompressor::compressFile(const std::string& inputFilename) {
    // Create a compression thread
    std::thread compressionThread(&FileCompressor::compressAndTime, this, inputFilename);
    compressionThread.detach(); // Detach the thread, don't block waiting for it
}

// Method to call pigz to compress a file
void FileCompressor::compressWithPigz(const std::string& inputFile) {
    std::string command = "pigz --force " + inputFile; // Force overwrite, Adjust options for parallelism if necessary
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Compression failed with pigz" << std::endl;
        LogFileWriter::getInstance().logError("ERROR: Compression failed with pigz: " + inputFile);
    } else {
        std::cout << "Compression completed with pigz: " << inputFile << std::endl;
        LogFileWriter::getInstance().logInfo("Compression completed with pigz: " + inputFile);
    }
}

// Method to compress a file and time the operation
void FileCompressor::compressAndTime(const std::string& inputFilename) {
    auto start = std::chrono::high_resolution_clock::now(); // Start timing

    compressWithPigz(inputFilename); // Perform compression

    auto end = std::chrono::high_resolution_clock::now(); // End timing
    std::chrono::duration<double, std::micro> elapsed = end - start; // Calculate elapsed time in microseconds

    LogFileWriter::getInstance().logInfo("Compression of " + inputFilename + " completed in " + std::to_string(elapsed.count()) + " microseconds.");
}
