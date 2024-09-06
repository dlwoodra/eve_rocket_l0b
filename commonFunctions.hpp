#ifndef COMMONFUNCTIONS_H
#define COMMONFUNCTIONS_H

#include "eve_l0b.hpp" // absorbed eve_structures.hpp
#include "CCSDSReader.hpp"
//#include "eve_megs_twoscomp.h"
//#include "eve_megs_pixel_parity.h"
#include "FITSWriter.hpp"
#include "FileInputSource.hpp"
#include "InputSource.hpp"
#include "LogFileWriter.hpp"
#include "RecordFileWriter.hpp"
#include "USBInputSource.hpp"

//#include <chrono> // included in RecordFileWriter.hpp
//#include <cstdint> // included in CCSDSReader.hpp
//#include <fstream> // included in CCSDSReader.hpp
//#include <iostream> // included in CCSDSReader.hpp
#include <string>
//#include <vector> // included in CCSDSReader.hpp


//prototypes
bool isValidFilename(const std::string& filename);

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

void printBytesToStdOut(const uint8_t* array, uint32_t start, uint32_t end);


#endif // COMMONFUNCTIONS_H