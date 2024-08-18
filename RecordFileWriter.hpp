#ifndef RECORD_FILE_WRITER_HPP
#define RECORD_FILE_WRITER_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "CCSDSReader.hpp"

//const uint32_t SYNC_MARKER = 0x1ACFFC1D; // Example sync marker

class RecordFileWriter {
public:

    std::string recordFilename;

    // Constructor that generates the filename based on the current date and time
    RecordFileWriter();

    // Destructor to close the file
    ~RecordFileWriter();

    // Method to write the sync marker and packet data to the file
    bool writeSyncAndPacketToRecordFile(const std::vector<uint8_t>& packet);

    std::string getRecordFilename() const;

    // Close the file if it's open
    void close();

    // Generate a filename based on the current date and time
    std::string generateFilename();

private:
    std::string outputFile;
    std::ofstream recordFile;

};

#endif // RECORD_FILE_WRITER_HPP
