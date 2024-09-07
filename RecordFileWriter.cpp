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

    if (!recordFile.is_open()) {
        std::cerr << "ERROR: File is not open for writing." << std::endl;
        return false; // File is not open, cannot write
    }

    // Check if we need to rotate the file
    if ( !checkAndRotateFile() ) {
        std::cout << "Info: writeSyncAndPacketToRecordFile received BAD status from checkAndRotateFile" << std::endl;
    }

    // Write sync marker to the file
    // on little endian a 32-bit write is byteswapped, 
    // use reversed sync to compensate
    recordFile.write(reinterpret_cast<const char*>(&BSWAP_SYNC_MARKER), sizeof(BSWAP_SYNC_MARKER));
    //std::cout << "writeSyncAndPacketToRecordFile wrote syncMarker" << std::endl;

    // Write packet data to the file
    recordFile.write(reinterpret_cast<char*>(const_cast<uint8_t*>(packet.data())), packet.size());

    return recordFile.good(); // Returns true if the write was successful
}

// Flush the file buffer
void RecordFileWriter::flush() {
    if (recordFile.is_open()) {
        recordFile.flush();
        std::cout << "Record file buffer flushed: " << outputFile << std::endl;
    }
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

        // create directory
    oss << std::put_time(&buf, "./record/%Y/%j/");
    std::string dirPath = oss.str();
    // Create the directories if they don't exist, use system call
    std::string mkdirCommand = "mkdir -p " + dirPath;
    if (system(mkdirCommand.c_str()) != 0) {
        std::cerr << "ERROR: Could not create directories for record file." << std::endl;
        return "";
    }

    oss << std::put_time(&buf, "record_%Y%j_%H%M%S") << ".rtlm";

    return oss.str();
}

std::string RecordFileWriter::getRecordFilename() const {
    return outputFile;
}

// Check if the current minute has rolled over and open a new file if it has
bool RecordFileWriter::checkAndRotateFile() {

    TimeInfo currentTime;
    int currentMinute = currentTime.getMinute();
    
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
