/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
*  target is XEM7310-a75
*/

#include "CCSDSReader.hpp"
#include "eve_megs_twoscomp.h"
#include "eve_megs_pixel_parity.h"
#include "eve_l0b.hpp"
#include "fileutils.hpp"
#include "FITSWriter.hpp"
#include "FileInputSource.hpp"
#include "InputSource.hpp"
#include "LogFileWriter.hpp"
#include "RecordFileWriter.hpp"
#include "USBInputSource.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

void print_help();
void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord);
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord);
void processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet);
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
    if (!skipRecord) {
        recordWriter = std::unique_ptr<RecordFileWriter>(new RecordFileWriter());
        // the c++14 way recordWriter = std::make_unique<RecordFileWriter>();
    }

    if (isValidFilename(filename)) {
        // read packets from the file provided in the argument list
        FileInputSource fileSource(filename);
        CCSDSReader fileReader(&fileSource);

        if (fileReader.open()) {
            processPackets(fileReader, recordWriter, skipRecord);
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
            LogFileWriter::getInstance().logInfo("Received filename arg: " + arg);
        } else if (arg == "--skipESP" || arg == "-skipESP") {
            skipESP = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
        } else if (arg == "--skipMP" || arg == "-skipMP") {
            skipMP = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
        } else if (arg == "--help" || arg == "-help") {
            print_help();
        } else if (arg == "--skipRecord" || arg == "-skipRecord") {
            skipRecord = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
        } else {
            LogFileWriter::getInstance().logError("Unknown command line option: " + arg);
            std::cerr << "Unknown option: " << arg << std::endl;
        }
    }
}

// reads a packet and writes it to the recordfile
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord) {
    std::vector<uint8_t> packet;
    int counter = 0;

    std::cout << "entered processPackets" << std::endl;
    while (pktReader.readNextPacket(packet)) {
        std::cout << "processPackets counter " << counter++ << std::endl;

        // Record packet if required
        // c++ guarantees evaluation order from left to right to support short-circuit evaluation
        if (!skipRecord && recordWriter && !recordWriter->writeSyncAndPacketToRecordFile(packet)) {
            LogFileWriter::getInstance().logError("ERROR: processPackets failed to write packet to record file.");
            return;
        } else {
            // this next line generates too many message to the screen
            //std::cout << "processPackets wrote to recordFilename " << recordWriter->getRecordFilename() << std::endl;
        }

        // Process packet
        processOnePacket(pktReader, packet);
    }
}

void processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet) {
    auto start = std::chrono::system_clock::now();

    auto header = std::vector<uint8_t>(packet.cbegin(), packet.cbegin() + PACKET_HEADER_SIZE);
    uint16_t apid = pktReader.getAPID(header);
    uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);
    uint16_t packetLength = pktReader.getPacketLength(header);

    auto payload = std::vector<uint8_t>(packet.cbegin() + PACKET_HEADER_SIZE, packet.cend());
    double timeStamp = pktReader.getPacketTimeStamp(payload);

    LogFileWriter::getInstance().logInfo("APID " + std::to_string(apid) + \
        " SSC " + std::to_string(sourceSequenceCounter) + \
        " pktLen " + std::to_string(packetLength) );
    //std::cout << "APID: " << apid << " SSC: " << sourceSequenceCounter << " pktLen:" << packetLength 
    //          << " timestamp: " << timeStamp << "\n"; //" mode:" << mode << std::endl;

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
    uint64_t elapsedMicrosec = 1.e6 * elapsed_seconds.count();
    LogFileWriter::getInstance().logInfo("Elapsed microsec "+ std::to_string(elapsedMicrosec));
}

uint32_t payloadToTAITimeSeconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("Payload must contain at least 4 bytes.");
    }

    return (static_cast<uint32_t>(payload[0]) << 24) |
           (static_cast<uint32_t>(payload[1]) << 16) |
           (static_cast<uint32_t>(payload[2]) << 8)  |
           static_cast<uint32_t>(payload[3]);
}

uint32_t payloadToTAITimeSubseconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 6) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("Payload must contain at least 4 bytes.");
    }

    return (static_cast<uint32_t>(payload[4]) << 24) |
           (static_cast<uint32_t>(payload[5]) << 16);
}

// the payload starts with the secondary header timestamp
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, 
    double timeStamp) {

    // insert pixels into image

    uint16_t year, doy, hh, mm, ss;
    uint32_t sod;
    uint32_t tai_sec;
    int8_t status=0;
    struct MEGS_IMAGE_REC megsImage;
    int vcdu_impdu_prihdr_length = 20;
    uint8_t pktarr[STANDARD_MEGSAB_PACKET_LENGTH + vcdu_impdu_prihdr_length];
    // vcdu has 14 bytes before the packet start (vcdu=6, impdu=8)
    // pkthdr=6 bytes before the payload starts at the 2nd hdr timestamp
    // copy from payload into pktarr starting at byte 20
    std::copy(payload.begin(), payload.end(), pktarr+vcdu_impdu_prihdr_length);

    tai_sec = payloadToTAITimeSeconds(payload);
    megsImage.tai_time_seconds = tai_sec;
    megsImage.tai_time_subseconds = payloadToTAITimeSubseconds(payload);

    tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss);
    std::cout << "called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<"\n";

    int parityErrors = assemble_image(pktarr, &megsImage, sourceSequenceCounter, &status);
    std::cout << "called assemble_image" << "\n";


    if ( parityErrors > 0 ) {
        std::cout << "assemble_image returned parity errors: " << parityErrors << "\n";
    }

    if ( sourceSequenceCounter == 2394) {
        std::cout<<"end of MEGS-A at 2394"<<"\n";

        // need to run this in another thread

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        if (fitsFileWriter) {
        //  if (!fitsFileWriter->writePacketToFITS(packet, apid, timeStamp)) {
        //    std::cerr << "ERROR: Failed to write packet to FITS file." << std::endl;
        //    return;
        //  }
        }
    }

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