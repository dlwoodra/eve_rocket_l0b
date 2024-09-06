#ifndef RECORD_FILE_WRITER_HPP
#define RECORD_FILE_WRITER_HPP

#include "CCSDSReader.hpp"
//#include <cstdint>  // in CCSDSReader.hpp
#include <cstdlib> // used for getenv
#include <chrono>
//#include <fstream>  // in CCSDSReader.hpp
#include <iomanip>
//#include <iostream>  // in CCSDSReader.hpp
#include <sstream>
#include <string>
//#include <vector>  // in CCSDSReader.hpp

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

    void flush();

    // Generate a filename based on the current date and time
    std::string generateRecordFilename();

    // Check if the current minute has rolled over and open a new file if it has
    bool checkAndRotateFile();

private:
    std::string outputFile;
    std::ofstream recordFile;

    int lastMinute;

};

#endif // RECORD_FILE_WRITER_HPP
