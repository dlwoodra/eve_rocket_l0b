#ifndef CCSDS_READER_HPP
#define CCSDS_READER_HPP

#include <cstdint>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "InputSource.hpp"
#include "FileInputSource.hpp"
#include "ProgramState.hpp"

constexpr double ONE_OVER_65536 = (double (1.0) / double (65536.0));

class CCSDSReader {
public:
  CCSDSReader(InputSource* source);
  ~CCSDSReader();

  bool open(); 
  void close();

  // Read the next CCSDS packet
  bool readNextPacket(std::vector<uint8_t>& packet); 

  //
  // inline functions must be implemented in the header
  //

  // get the APID from the header
  // Least significant 11-bits of first 2 header bytes
  inline uint16_t getAPID(const std::vector<uint8_t>& header) {
    return ((uint16_t (header[0]) & 0x07) << 8) | uint16_t (header[1]); 
  }

  // helper method to get the packet length from the primary header
  // CCSDS primary header contains the length excluding primary header
  inline uint16_t getPacketLength(const std::vector<uint8_t>& header) {
    return uint16_t (header[4] << 8) | header[5];
  }

  // helper method to get the source sequence counter from the primary header
  // CCSDS primary header contains the 14-bits of the source sequence counter
  inline uint16_t getSourceSequenceCounter(const std::vector<uint8_t>& header) {
    return ((static_cast<uint16_t> (header[2]) & 0x3F) << 8) | static_cast<uint16_t> (header[3]);
  }

  inline uint16_t getMode(const std::vector<uint8_t>& payload) {
    const uint16_t offset = 8; // skip 8 bytes from timestamp 
    return (uint16_t (payload[offset])<< 8) | uint16_t (payload[offset+1]); 
  }

  double getPacketTimeStamp(const std::vector<uint8_t>& payload); 

private:
    InputSource* source;

    // Locate the sync marker in the binary file
    bool findSyncMarker();

    // helper method to read the 6-byte CCSDS primary packet header
    bool readPacketHeader(std::vector<uint8_t>& header); 

};

#endif // CCSDS_READER_HPP
