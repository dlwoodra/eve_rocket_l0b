/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
*
*  Usage: 
*/
#include "RecordFileWriter.hpp"
#include "fileutils.hpp"
#include "CCSDSReader.hpp"

#include <iostream>
#include <iomanip>
#include <cstring> //for strlen
#include <fstream>
#include <chrono>
#include <ctime>
#include <vector>
#include <cstdint>


/* include opal kelly fronpanel */
//#include "okFrontPanelDLL.h"
// recall <> search only the standard directory
// double quotes search current directory first, then the standard directory

//std::ofstream openRecordFilename();

// *********************************************************************/
// Settings
// *********************************************************************/
#define MAX_DEAD_TIME_MS     200

// *********************************************************************/
// Globals
// *********************************************************************/

// pointer to front panel object
//OpalKelly::FrontPanelDevices devices;
//OpalKelly::FrontPanelPtr devptr;
//okCFrontPanel *dev;
//okTDeviceInfo  m_devInfo;



// main follows unix standard where returning 0 is good
// accept a filename argument and switches start with "-" or "--"
int main(int argc, char* argv[]) {

  // Filename initialized as an empty string
  std::string filename = "";
  bool useFile = false;
  
  // Boolean switches initialized to false
  bool skipESP = false;
  bool skipMP = false;
  bool skipRecord = false;

  // Parse arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
 
    // If the filename hasn't been set yet and the argument doesn't start with a dash, treat it as the filename
    if (isValidFilename(arg) && arg[0] != '-') {
      filename = arg;
      useFile = true;
      std::cout << "isValidFileName result: " << isValidFilename(filename) << std::endl;
    } else if (arg == "--skipESP" || arg == "-skipESP") {
      skipESP = true;
    } else if (arg == "--skipMP" || arg == "-skipMP") {
      skipMP = true;
    } else if (arg == "--skipRecord" || arg == "-skipRecord") { // disable record file
      skipRecord = true;
    } else {
      std::cerr << "Unknown option: " << arg << std::endl;
    }
  }

  // Output the parsed information
  std::cout << "filename: " << (isValidFilename(filename) ? filename : "None") << std::endl;
  std::cout << "skipESP: " << (skipESP ? "ON" : "OFF") << std::endl;
  std::cout << "skipMP: " << (skipMP ? "ON" : "OFF") << std::endl;
  std::cout << "skipRecord: " << (skipRecord ? "ON" : "OFF") << std::endl;

  std::cout << "variable useFile: " << (useFile) << std::endl;

  //auto currentClock = std::chrono::system_clock::now(); 
  //std::time_t currentTime = std::chrono::system_clock::to_time_t(currentClock);
  //std::cout <<  "finished at" << std::ctime(&CurrentTime) << std::endl;

  std::unique_ptr<RecordFileWriter> writer;
  if (!skipRecord) {
    writer = std::make_unique<RecordFileWriter>();
  }

  if (isValidFilename(filename)) {
  
    CCSDSReader pktReader(filename);

    if(!pktReader.open()) {
      std::cerr << "Failed to open file." << std::endl;
      return 1;
    }

    //static uint32_t syncMarker = SYNC_MARKER;

    std::vector<uint8_t> packet;
    while (pktReader.readNextPacket(packet)) {

      // record all packet data into the recordFile
      if (!skipRecord) {
        if (writer) { // Check is writer is not null
          if (!writer->writeSyncAndPacketToRecordFile(packet)) {
            std::cout << "ERROR: Failed to write packet to record file." << std::endl;
            exit(1); // Fatal error
          }
        }
      }

      // start timing
      auto start = std::chrono::system_clock::now();
 
      // get the packet header
      std::vector<uint8_t> header(packet.begin(), packet.begin() + PACKET_HEADER_SIZE);
      
      uint16_t apid = pktReader.getAPID(header);
      
      uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);
      
      std::vector<uint8_t> payload(packet.begin() + PACKET_HEADER_SIZE, packet.end());

      double timeStamp = pktReader.getPacketTimeStamp(payload);

      //std::cout << "Read a packet of size: " <<packet.size() << " bytes." << std::endl;
      std::cout << "APID: " << apid << " SSC: " 
        << sourceSequenceCounter << " timestamp:" 
        << timeStamp << std::endl;

      // debug - display hex values from start of packet
      for (int32_t i = 0; i < 25; ++i) {
        std::cout << std::hex <<std::setw(2) << std::setfill('0') << static_cast<int>(packet[i]) << " ";
      }
      std::cout << std::endl;

      // process the packet based on APID and time

      // get timing result for packet read and processing
      auto end = std::chrono::system_clock::now(); 
      std::chrono::duration<double> elapsed_seconds = end-start;
      std::cout << "elapsed time: " << elapsed_seconds.count() << " sec" << std::endl;

    }
    if (!useFile) {
      pktReader.close();
    }

    // unique_ptr will clean up all it's references - no cleanup needed

    return 0;

  } //isValidFilename


  // to convert the clock time to something like Day Mon dd hh:mm:ss yyyy
  auto end = std::chrono::system_clock::now(); 
  std::time_t end_time = std::chrono::system_clock::to_time_t(end);
  std::cout <<  "finished at" << std::ctime(&end_time) << std::endl;
    
  return 0;
}
