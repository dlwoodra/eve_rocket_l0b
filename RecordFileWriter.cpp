#include "RecordFileWriter.hpp"

// Constructor that generates the filename based on the current date and time
RecordFileWriter::RecordFileWriter()
    : outputFile(generateFilename()), recordFile(outputFile, std::ios::binary) {
    if (!recordFile.is_open()) {
        std::cerr << "ERROR: Failed to open output file: " << outputFile << std::endl;
        exit(1); // fatal, exit
    }

    std::cout << "Record file opened: " << outputFile << std::endl;
}

// Destructor to close the file
RecordFileWriter::~RecordFileWriter() {
    close();
}

// Method to write the sync marker and packet data to the file
bool RecordFileWriter::writeSyncAndPacketToRecordFile(const std::vector<uint8_t>& packet) {
    static uint32_t syncMarker = SYNC_MARKER;

    // Write sync marker to the file
    recordFile.write(reinterpret_cast<char*>(&syncMarker), sizeof(syncMarker));

    // Write packet data to the file
    recordFile.write(reinterpret_cast<char*>(const_cast<uint8_t*>(packet.data())), packet.size());

    return recordFile.good(); // Returns true if the write was successful
}

// Close the file if it's open
void RecordFileWriter::close() {
    if (recordFile.is_open()) {
        recordFile.close();
        std::cout << "Record file closed: " << outputFile << std::endl;
    }
}

// Generate a filename based on the current date and time
std::string RecordFileWriter::generateFilename() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm buf;
    localtime_r(&in_time_t, &buf);

    std::ostringstream oss;
    oss << std::put_time(&buf, "record_%Y_%j_%H_%M_%S") << ".rtlm";

    return oss.str();
}

std::string RecordFileWriter::getRecordFilename() const {
    return outputFile;
}