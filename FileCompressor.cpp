#include "FileCompressor.hpp"

// Public method to start the compression in a separate thread
void FileCompressor::compressFile(const std::string& inputFilename) {
    // Create a compression thread
    std::thread compressionThread(&FileCompressor::compressAndTime, this, inputFilename);
    if ( !globalState.running.load() ) {
        compressionThread.join(); // Wait for the thread to finish
    } else {
    compressionThread.detach(); // Detach the thread, don't block waiting for it
    }
}

// void FileCompressor::compressFile(const std::string& inputFilename) {
//     std::lock_guard<std::mutex> lock(compressorThreadMutex); // Lock while adding a thread
//     compressionThreads.emplace_back(&FileCompressor::compressAndTime, this, inputFilename);
// }

// void FileCompressor::waitForAllThreads() {
//     std::lock_guard<std::mutex> lock(compressorThreadMutex); // Lock while joining threads
//     for (auto& thread : compressionThreads) {
//         if (thread.joinable()) {
//             thread.join();
//         }
//     }
//     compressionThreads.clear();
// }

// Method to call pigz to compress a file
void FileCompressor::compressWithPigz(const std::string& inputFile) {
    std::string command = "pigz --force " + inputFile; // Force overwrite, Adjust options for parallelism if necessary
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Compression failed with pigz" << std::endl;
        LogFileWriter::getInstance().logError("ERROR: Compression failed with pigz: " + inputFile);
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
