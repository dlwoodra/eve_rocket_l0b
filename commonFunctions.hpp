#ifndef COMMONFUNCTIONS_H
#define COMMONFUNCTIONS_H

#include "eve_l0b.hpp" // absorbed eve_structures.hpp
#include "CCSDSReader.hpp"
#include "FITSWriter.hpp"
#include "FileInputSource.hpp"
#include "InputSource.hpp"
#include "LogFileWriter.hpp"
#include "RecordFileWriter.hpp"
#include "USBInputSource.hpp"
#include "ProgramState.hpp"
#include <functional> // for convertSHKData lambda polynomial function
#include <array> // for std::array
#include <omp.h> // for OpenMP

//#include <chrono> // included in RecordFileWriter.hpp
//#include <cstdint> // included in CCSDSReader.hpp
//#include <fstream> // included in CCSDSReader.hpp
//#include <iostream> // included in CCSDSReader.hpp
#include <algorithm> // needed for std::copy
#include <string>
#include <sys/stat.h> // For mkdir
#include <sys/types.h> // For mode_t
#include <errno.h>     // For errno
#include <cstring>     // For strerror

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

void printBytes(const void* ptr, size_t size);
void printBytesToStdOut(const uint8_t* array, uint32_t start, uint32_t end);
void printUint16ToStdOut(const uint16_t* image, size_t size, size_t count);

bool create_directory_if_not_exists(const std::string& dirPath);
bool create_single_directory(const std::string& path);
std::vector<uint16_t> transposeImageTo1D(const uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH]);
uint32_t payloadBytesToUint32(const std::vector<uint8_t>& payload, const int32_t offsetByte);
uint32_t payloadToTAITimeSeconds(const std::vector<uint8_t>& payload);
uint32_t payloadToTAITimeSubseconds(const std::vector<uint8_t>& payload);

double tai_ss(uint32_t tai_secods, uint32_t tai_subseconds);

void countSaturatedPixels(const uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH],
                          uint32_t& saturatedPixelsTop,
                          uint32_t& saturatedPixelsBottom,
                          bool testPattern = false);

#endif // COMMONFUNCTIONS_H