/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
target XEM7310-a75
*/

#include "CCSDSReader.hpp"
#include "eve_megs_twoscomp.h"
#include "eve_megs_pixel_parity.h"
#include "eve_l0b.hpp"
#include "fileutils.hpp"
#include "FITSWriter.hpp"
#include "RecordFileWriter.hpp"
#include "InputSource.hpp"
#include "FileInputSource.hpp"
#include "USBInputSource.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

void print_help();
void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord);
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, std::unique_ptr<FITSWriter>& fitsFileWriter, bool skipRecord);
void processPacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet);
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void processMegsBPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void processMegsPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void processHKPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
std::string SelectUSBSerialNumber();

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
        } else {
            std::cerr << "Failed to open file." << std::endl;
            return EXIT_FAILURE;  
        }
        fileReader.close();

    } else {
        // read packets from USB

        // THIS IS JUST A STUB
        std::string serialNumber; 
        USBInputSource usbSource(serialNumber);

        // create a CCSDSReader instance        
        CCSDSReader usbReader(&usbSource);
        std::cout << "main: Created CCSDSReader usbReader object."  << std::endl;

        //loop
        usbSource.CGProcRx(); // receive, does not return until disconnect

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
    int counter = 0;

    std::cout << "entered processPackets" << std::endl;
    while (pktReader.readNextPacket(packet)) {
        std::cout << "processPackets counter " << counter++ << std::endl;

        // Record packet if required
        // c++ guarantees evaluation order from left to right to support short-circuit evaluation
        if (!skipRecord && recordWriter && !recordWriter->writeSyncAndPacketToRecordFile(packet)) {
            std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
            return;
        } else {
            // this next line generates too many message to the screen
            //std::cout << "processPackets wrote to recordFilename " << recordWriter->getRecordFilename() << std::endl;
        }

        // Process packet
        processPacket(pktReader, packet);
    }
}

void processPacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet) {
    auto start = std::chrono::system_clock::now();

    auto header = std::vector<uint8_t>(packet.cbegin(), packet.cbegin() + PACKET_HEADER_SIZE);
    uint16_t apid = pktReader.getAPID(header);
    uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);
    uint16_t packetLength = pktReader.getPacketLength(header);

    auto payload = std::vector<uint8_t>(packet.cbegin() + PACKET_HEADER_SIZE, packet.cend());
    double timeStamp = pktReader.getPacketTimeStamp(payload);
    //uint16_t mode = pktReader.getMode(payload);

    std::cout << "APID: " << apid << " SSC: " << sourceSequenceCounter << " pktLen:" << packetLength 
              << " timestamp: " << timeStamp << "\n"; //" mode:" << mode << std::endl;

    switch (apid) {
        case MEGSA_APID:
            processMegsAPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSB_APID:
            processMegsBPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case ESP_APID:
            processESPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSP_APID:
            processMegsPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case HK_APID:
            processHKPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        default:
            std::cerr << "Unrecognized APID: " << apid << std::endl;
            // Handle error or unknown APID case if necessary
            break;
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Elapsed time: " << elapsed_seconds.count() << " sec" << std::endl;
}

// the payload starts with the secondary header timestamp
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    // insert pixels into image

    int8_t status=0;
    struct MEGS_IMAGE_REC megsImage;
    uint8_t pktarr[STANDARD_MEGSAB_PACKET_LENGTH];

    int parityErrors = assemble_image(pktarr, &megsImage, &status);

    if ( parityErrors > 0 ) {
        std::cout << "assemble_image returned parity errors: " << parityErrors << "\n";
    }

    // Write packet data to a FITS file if applicable
    //if (fitsFileWriter) {
    //  if (!fitsFileWriter->writePacketToFITS(packet, apid, timeStamp)) {
    //    std::cerr << "ERROR: Failed to write packet to FITS file." << std::endl;
    //    return;
    //  }
    //}
}
void processMegsBPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}
void processMegsPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}
void processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}

void processHKPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
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