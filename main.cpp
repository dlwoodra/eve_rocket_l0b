/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
target XEM7310-a75
*/

#include "CCSDSReader.hpp"
#include "fileutils.hpp"
#include "FITSWriter.hpp"
#include "RecordFileWriter.hpp"
#include "InputSource.hpp"
#include "FileInputSource.hpp"
#include "USBInputSource.hpp"

//#include "okFrontPanelDLL.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord);
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, std::unique_ptr<FITSWriter>& fitsFileWriter, bool skipRecord);
void print_help();

int main(int argc, char* argv[]) {
    std::string filename;
    bool skipESP = false;
    bool skipMP = false;
    bool skipRecord = false;

    parseCommandLineArgs(argc, argv, filename, skipESP, skipMP, skipRecord);

    std::unique_ptr<RecordFileWriter> recordWriter;
    std::unique_ptr<FITSWriter> fitsFileWriter;
    if (!skipRecord) {
        recordWriter = std::make_unique<RecordFileWriter>();
        fitsFileWriter = std::make_unique<FITSWriter>();
    }

    if (isValidFilename(filename)) {
        // read packets from the file provided in the argument list
        FileInputSource fileSource(filename);
        CCSDSReader fileReader(&fileSource);

        if (fileReader.open()) {
            processPackets(fileReader, recordWriter, fitsFileWriter, skipRecord);
        }
        fileReader.close();

        if (!fileReader.open()) {
            std::cerr << "Failed to open file." << std::endl;
            return EXIT_FAILURE;
        }

    } else {
        // read packets from USB

        // THIS IS JUST A STUB
        std::string serialNumber = "12345678"; //Need to replace!!!
        USBInputSource usbSource(serialNumber);
        CCSDSReader usbReader(&usbSource);
        processPackets(usbReader, recordWriter, fitsFileWriter, 0); // always record from USB
        usbReader.close();

    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::cout << "Finished at " << std::ctime(&end_time) << std::endl;

    return EXIT_SUCCESS;
}

void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (isValidFilename(arg) && arg[0] != '-') {
            filename = arg;
        } else if (arg == "--skipESP" || arg == "-skipESP") {
            skipESP = true;
        } else if (arg == "--skipMP" || arg == "-skipMP") {
            skipMP = true;
        } else if (arg == "--help" || arg == "-help") {
            print_help();
        } else if (arg == "--skipRecord" || arg == "-skipRecord") {
            skipRecord = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
        }
    }
}

void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, std::unique_ptr<FITSWriter>& fitsFileWriter, bool skipRecord) {
    std::vector<uint8_t> packet;

    int counter=0;
    while (pktReader.readNextPacket(packet)) {

        std::cout<< "processPackets counter " << counter++ << std::endl;

        if (!skipRecord && recordWriter) {
            if (!recordWriter->writeSyncAndPacketToRecordFile(packet)) {
                std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
                return;
            }
            //std::cout << "processPackets wrote to recordFilename " << recordWriter->getRecordFilename() << std::endl;
        }

        auto start = std::chrono::system_clock::now();

        std::vector<uint8_t> header(packet.begin(), packet.begin() + PACKET_HEADER_SIZE);
        uint16_t apid = pktReader.getAPID(header);
        uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);

        uint16_t packetLength = pktReader.getPacketLength(header);

        std::vector<uint8_t> payload(packet.begin() + PACKET_HEADER_SIZE, packet.end());
        double timeStamp = pktReader.getPacketTimeStamp(payload);
        uint16_t mode = pktReader.getMode(payload);
 
        std::cout << "APID: " << apid << " SSC: " << sourceSequenceCounter << " pktLen:" << packetLength << " timestamp: " << timeStamp << " mode:" << mode << std::endl;

        // Write packet data to a FITS file if applicable
        //if (fitsFileWriter) {
        //  if (!fitsFileWriter->writePacketToFITS(packet, apid, timeStamp)) {
        //    std::cerr << "ERROR: Failed to write packet to FITS file." << std::endl;
        //    return;
        //  }
        //}

        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        //std::cout << "Elapsed time: " << elapsed_seconds.count() << " sec" << std::endl;
    }
}

void print_help() {
  std::cout << "Main c++ program to support SDO EVE Rocket calibration at SURF." << std::endl;
  std::cout << "Compiled: " << __DATE__ << " " << __TIME__ << std::endl;
  std::cout << "Compiler: " << __VERSION__ << std::endl;
  std::cout << "Basefile: " << __BASE_FILE__ << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << " ./ql [tlmfilename] [options]" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Options: " << std::endl;
  std::cout << " -help runs print_help to display this message and exit" << std::endl;
  std::cout << " -skipESP will ignore ESP packets (apid 605)" << std::endl;
  std::cout << " -skipMP will ignore MEGS-P packets (apid 604)" << std::endl;
  std::cout << " -skipRecord disable recording of telemetry to a file" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "When provided, tlmfilename is a binary file of sync_marker,packet pairs. " << std::endl;
  std::cout << "Recorded files can be played back this way. " << std::endl;
  std::cout << "When using tlmfilename, -skipRecord will avoid creating additional files." << std::endl;
  std::cout << "The -skipRecord option should not be used for opalkelly connections." << std::endl;
  exit(EXIT_FAILURE);
}