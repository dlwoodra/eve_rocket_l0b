#ifndef CCSDS_READER_HPP
#define CCSDS_READER_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "InputSource.hpp"
#include "FileInputSource.hpp"
#include "USBInputSource.hpp"

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
  CCSDSReader(InputSource* source);
  ~CCSDSReader(); //{ close(); }

  bool open(); //{
  //    return source->open();
  //}       // Opens the binary file or sets up USB

  void close(); // {
  //  source->close(); 
  //}      // Close the binary file, reset the USB port

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

  // end of public inline functions

  double getPacketTimeStamp(const std::vector<uint8_t>& payload); 



private:
    InputSource* source;

    // Locate the sync marker in the binary file
    bool findSyncMarker();

    // helper method to read the 6-byte CCSDS primary packet header
    bool readPacketHeader(std::vector<uint8_t>& header); 

    template<typename T>
    T readValue(); // Helper function to read a value of type T from the file
};

#endif // CCSDS_READER_HPP
