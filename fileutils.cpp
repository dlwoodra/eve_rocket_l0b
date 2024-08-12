#include <string>
#include <iostream>
#include <string>
#include <fstream>
#include "CCSDSReader.hpp"

// Function returns false if filename is empty
bool isValidFilename(const std::string& filename) {
    return !filename.empty();
}

/*
std::ofstream openRecordFilename() {

  std::string outputFile = "testRecord.rtlm";

  std::ofstream outFile(outputFile, std::ios::binary);

  if (!outFile.is_open()) {
    std::cerr << "ERROR: openRecordFile - failed to open output file: " << outputFile <<std::endl;
    exit(1); // fatal, exit
  }

  std::cout << "Record file opened: " << outputFile << std::endl;
  return outFile;
}
*/

/*
bool writeSyncAndPackeToRecordFile(std::vector<uint8_t>& packet) {

  static uint32_t syncMarker = SYNC_MARKER;

  std::ofstream recordFile = openRecordFilename();

  // record all packet data into the recordFile
  recordFile.write(reinterpret_cast<char*>(&syncMarker), sizeof(syncMarker));
  recordFile.write(reinterpret_cast<char*>(packet.data()), packet.size());

  return(0);
}
*/

/*
bool closeRecordFile() {
  recordFile.close();
}
*/