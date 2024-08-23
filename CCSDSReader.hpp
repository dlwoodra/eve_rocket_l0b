#ifndef CCSDS_READER_HPP
#define CCSDS_READER_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "InputSource.hpp"

#define ONE_OVER_65536 (1.0 / 65536.0)

// Constants
const uint32_t SYNC_MARKER = 0x1ACFFC1D; // Example sync marker, typical for CCSDS frames
const uint32_t BSWAP_SYNC_MARKER = 0x1DFCCF1A;
const size_t PACKET_HEADER_SIZE = 6;     // CCSDS packet primary header size in bytes
const uint16_t STANDARD_MEGSAB_PACKET_LENGTH = 1761; // value from packet (one less than size)
const uint16_t STANDARD_MEGSP_PACKET_LENGTH = 25; // apid 604
const uint16_t STANDARD_ESP_PACKET_LENGTH = 89; // apid 605
const uint16_t STANDARD_HK_PACKET_LENGTH = 265; // apid 606
// David's definitions of APIDs
const uint16_t MEGSA_APID = 601; // same as SDO EVE
const uint16_t MEGSB_APID = 602; // same as SDO EVE
const uint16_t MEGSP_APID = 604; // was SHK in SDO EVE
const uint16_t ESP_APID = 605; // not used in SDO EVE, MEGS-P and ESP were both in 603
const uint16_t HK_APID = 606; // rocket housekeeping only
const uint8_t mask6bit = 0x3f; // upper byte of source sequence counter

class CCSDSReader {
public:
  CCSDSReader(InputSource* source) : source(source) {}
  ~CCSDSReader() { close(); }

  bool open() {
      return source->open();
  }       // Opens the binary file or sets up USB

  void close() {
    source->close(); 
  }      // Close the binary file, reset the USB port

  // Read the next CCSDS packet
  bool readNextPacket(std::vector<uint8_t>& packet) {
    if (!findSyncMarker()) {
      std::cerr << "ERROR: CCSDSReader::readNextPacket did not find sync marker" << std::endl;
      return false;
    }

    std::vector<uint8_t> header(PACKET_HEADER_SIZE);
    if (!readPacketHeader(header)) {
      std::cerr << "ERROR: CCSDSReader::readNextPacket did not read the packet header" << std::endl;
      return false;
    }

    uint16_t packetLength = getPacketLength(header);

    if ((packetLength != STANDARD_MEGSAB_PACKET_LENGTH) && \
        (packetLength != STANDARD_ESP_PACKET_LENGTH) && \
        (packetLength != STANDARD_HK_PACKET_LENGTH) && \
        (packetLength != STANDARD_MEGSP_PACKET_LENGTH)) {
          std::cout << "ERROR: CCSDSREADER::readNextPacket has unexpected packetLength " << packetLength << std::endl;
          std::cout << " CCSDSREADER::readNextPacket APID with unexpected packetLength " << getAPID(header) << std::endl;
          return false;
    }

    // Allocate space for the packet
    packet.resize(PACKET_HEADER_SIZE + packetLength + 1);

    // Copy primary header to packet
    std::memcpy(packet.data(), header.data(), PACKET_HEADER_SIZE);

    // Debugging output: print header in hex
    //std::cout << "Header: ";
    //for (size_t i = 0; i < PACKET_HEADER_SIZE; ++i) {
    //    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(header[i]) << " ";
    //}
    //std::cout << std::endl;

    // Read the packet data
    if (!source->read(packet.data() + PACKET_HEADER_SIZE, packetLength + 1)) {
      std::cerr << "ERROR: CCSDSReader::readNextPacket failed to read data" << std::endl;
      return false;
    }
    return true;
  }

  //uint16_t getSourceSequenceCounter(const std::vector<uint8_t>& header);
  // helper method to get the source sequence counter from the primary header
  uint16_t getSourceSequenceCounter(const std::vector<uint8_t>& header) {
    //CCSDS primary header contains the 14-bits of the source sequence counter
    uint16_t sourceSequenceCounter = ((static_cast<uint16_t>(header[2]) & 0x3F) << 8) | static_cast<uint16_t>(header[3]);
    return sourceSequenceCounter;
  }

  //uint16_t getAPID(const std::vector<uint8_t>& header);
  // get the APID from the header
  uint16_t getAPID(const std::vector<uint8_t>& header) {
    // Least significant 11-bits of first 2 header bytes
    uint16_t apid = ((static_cast<uint16_t>(header[0]) & 0x07) << 8) | static_cast<uint16_t>(header[1]); 
    return apid;
  }

  double getPacketTimeStamp(const std::vector<uint8_t>& payload) {

    uint32_t seconds;
    uint16_t subseconds;
    uint32_t offset = 0;
    double timestamp = 0.0;

    // Use static_cast to enforce data types 
    seconds = (static_cast<uint32_t>(payload[offset]) << 24) | 
      (static_cast<uint32_t>(payload[offset+1]) << 16) | 
      (static_cast<uint32_t>(payload[offset+2]) << 8) | 
      static_cast<uint32_t>(payload[offset+3]);
    subseconds = (static_cast<uint16_t>(payload[offset+4]) << 8) | 
      static_cast<uint16_t>(payload[offset+5]);
    // byte 6 and 7 are unused in the rocket fpga, only MSB 16-bits contain subseconds
    // byte 6 and 7 are allocated but unused
    timestamp = static_cast<double>(seconds) + 
      (static_cast<double>(subseconds) * ONE_OVER_65536); //multiplcation is faster than division
    return timestamp;
  }

  uint16_t getMode(const std::vector<uint8_t>& payload) {

    uint16_t mode;
    uint32_t offset = 8; // skip 8 bytes from timestamp 

    mode = (static_cast<uint16_t>(payload[offset]<< 8)) | static_cast<uint16_t>(payload[offset+1]);

    return mode; 
    }

    // Calculate packet length
    uint16_t getPacketLength(const std::vector<uint8_t>& header) {
        uint16_t packetLength = (header[4] << 8) | header[5];
        return packetLength;
    }

private:
    InputSource* source;

    // Locate the sync marker in the binary file
    bool findSyncMarker() {
        uint32_t buffer = 0;
        uint8_t onebyte = 0;
        int bytecounter = 0;

        while (buffer != SYNC_MARKER) {
            if (source->read(&onebyte, sizeof(onebyte))) {
                buffer = (static_cast<uint32_t>(buffer) << 8) | static_cast<uint32_t>(onebyte);
                /* std::cout << "buffer value is " << std::hex << buffer << std::endl; */
                if (bytecounter++ > 4) {
                    std::cout << "ERROR findSyncMarker read more than 4 bytes" << std::endl;
                }
            } else {
                return false; // eof
            }
        }
        return true;
    }

    // helper method to read the 6-byte CCSDS primary packet header
    bool readPacketHeader(std::vector<uint8_t>& header) {
        return source->read(header.data(), PACKET_HEADER_SIZE); // fails on eof or error
    }

    template<typename T>
    T readValue(); // Helper function to read a value of type T from the file
};

#endif // CCSDS_READER_HPP
