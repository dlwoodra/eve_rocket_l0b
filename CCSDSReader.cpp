#include "CCSDSReader.hpp"
#include <cstring>
#include <iostream>

// initialize to use filename
CCSDSReader::CCSDSReader(const std::string& filename) : filename(filename) {}

// close the file if it is open
CCSDSReader::~CCSDSReader() {
  close();
}

// open file in binary mode, true if successful
bool CCSDSReader::open() {
  file.open(filename, std::ios::binary);
  //std::cout << "ERROR: CCSDSREADER::open status is " << file.is_open() << std::endl;
  return file.is_open();
}

// close the file if it is open
void CCSDSReader::close() {
  if (file.is_open()) {
    //std::cout << "ERROR: CCSDSREADER::close is closing" << std::endl;
    file.close();
  }
}

// find the sync marker
bool CCSDSReader::findSyncMarker() {

  uint32_t buffer = 0;
  uint8_t onebyte = 0;

  // read one byte at a time shifting 8 bits each iteration - endian-safe
  while (buffer != SYNC_MARKER) {
    if (file.read(reinterpret_cast<char*>(&onebyte), sizeof(onebyte))) {
      buffer = (buffer <<8) | onebyte;    
      /* std::cout << "buffer value is " << std::hex << buffer << std::endl; */
    } else { 
      return false; // eof
    }
  }
  return true;
}

// read the sync maker, packet header, and packet data
bool CCSDSReader::readNextPacket(std::vector<uint8_t>& packet) {
  if (!findSyncMarker()) {
    //std::cout << "ERROR: CCSDSREADER::readNextPacket did not find sync marker" << std::endl;
    return false;
  }

  std::vector<uint8_t> header(PACKET_HEADER_SIZE);
  if (!readPacketHeader(header)) {
    //std::cout << "ERROR: CCSDSREADER::readNextPacket has bad header " << std::endl;
    return false; // failed to read a packet header
  }

  //packet.sourceSequenceCounter = getSourceSequenceCounter(header);

  uint16_t packetLength = getPacketLength(header);
  if (packetLength != STANDARD_PACKET_LENGTH) {
    return false;
  }
  packet.resize(PACKET_HEADER_SIZE + packetLength + 1) ; // Allocate space for the packet

  std::memcpy(packet.data(), header.data(), PACKET_HEADER_SIZE); // Copy primary header to packet

  if (!file.read(reinterpret_cast<char*>(packet.data() + PACKET_HEADER_SIZE), packetLength + 1)) {
    //std::cout << "ERROR: CCSDSREADER::readNextPacket read error " << std::endl;
    return false; // Failed to read the packet data
  }
  return true;
}

// helper method to read the primary packet header
bool CCSDSReader::readPacketHeader(std::vector<uint8_t>& header) {
  if (file.read(reinterpret_cast<char*>(header.data()), PACKET_HEADER_SIZE)) {
    return true; //read success
  } else {
    return false; // read failed (eof or error)
  }
}

// helper method to get the packet length from the primary header
uint16_t CCSDSReader::getPacketLength(const std::vector<uint8_t>& header) {
  //CCSDS primary header contains the length excluding primary header
  uint16_t packetLength = (header[4] << 8) | header[5];
  return packetLength;
}

// helper method to get the source sequence counter from the primary header
uint16_t CCSDSReader::getSourceSequenceCounter(const std::vector<uint8_t>& header) {
  //CCSDS primary header contains the 14-bits of the source sequence counter
  uint16_t sourceSequenceCounter = ((header[2] & mask6bit) << 8) | header[3];
  return sourceSequenceCounter;
}

template<typename T>
T CCSDSReader::readValue() {
  T value;
  file.read(reinterpret_cast<char*>(&value), sizeof(T));
  return value;
}
