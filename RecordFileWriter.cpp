#include "RecordFileWriter.hpp"
#include "TimeInfo.hpp"

int recordFileMinute=-1;

// Constructor that generates the filename based on the current date and time
RecordFileWriter::RecordFileWriter()
    : outputFile(generateRecordFilename()), recordFile(outputFile, std::ios::binary) {
    if (!recordFile.is_open()) {
        std::cerr << "ERROR: Failed to open output file: " << outputFile << std::endl;
        exit(1); // fatal, exit
    }

    TimeInfo currentTime;
    recordFileMinute = currentTime.getMinute();

    std::cout << "Record file opened: " << outputFile << std::endl;
    std::cout << "recordFileMinute " << recordFileMinute << std::endl;
}

// Destructor to close the file
RecordFileWriter::~RecordFileWriter() {
    close();
}

// Method to write the sync marker and packet data to the file
bool RecordFileWriter::writeSyncAndPacketToRecordFile(const std::vector<uint8_t>& packet) {
    //static uint32_t syncMarker = SYNC_MARKER;
    static uint32_t syncMarker = BSWAP_SYNC_MARKER;

    if (!recordFile.is_open()) {
        std::cerr << "ERROR: File is not open for writing." << std::endl;
        return false; // File is not open, cannot write
    }

    // Check if we need to rotate the file
    if ( checkAndRotateFile() == true ) {
        //std::cout << "Info: writeSyncAndPacketToRecordFile received good status from checkAndRotateFile" << std::endl;
    } else {
        std::cout << "Info: writeSyncAndPacketToRecordFile received BAD status from checkAndRotateFile" << std::endl;
    }

    // Write sync marker to the file
    //recordFile.write(reinterpret_cast<char*>(&syncMarker), sizeof(syncMarker));
    recordFile.write(reinterpret_cast<char*>(&syncMarker), sizeof(syncMarker));
    //std::cout << "writeSyncAndPacketToRecordFile wrote syncMarker" << std::endl;

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
std::string RecordFileWriter::generateRecordFilename() {
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

// Check if the current minute has rolled over and open a new file if it has
bool RecordFileWriter::checkAndRotateFile() {

    //static int lastMinute = -1; // out of bounds

    TimeInfo currentTime;
    int currentMinute = currentTime.getMinute();
    

    //std::cout << "checkAndRotateFile currentMinute:" << currentMinute << std::endl; 
    //std::cout << "checkAndRotateFile recordFileMinute:" << recordFileMinute << std::endl;

    if ((recordFileMinute == -1) || (recordFileMinute != currentMinute)) {
        // The minute has changed, close the current file and open a new one
        if (lastMinute != -1) { close(); } // Close the old file
        outputFile = generateRecordFilename(); // Generate new filename
        recordFile.open(outputFile, std::ios::binary); // Open the new file

        if (!recordFile.is_open()) {
            std::cerr << "ERROR: Failed to open output file: " << outputFile << std::endl;
            exit(1); // fatal, exit
        }

        std::cout << "Record file rotated: " << outputFile << std::endl;
        recordFileMinute = currentMinute;
    } // otherwise the minute has not changed, keep writing to the same recordFile

    return true;
}
