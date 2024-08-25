#include "CCSDSReader.hpp"

#define ONE_OVER_65536 (1.0 / 65536.0)

// initialize to use filename
CCSDSReader::CCSDSReader(InputSource* source) : source(source) {}

// close the file if it is open
CCSDSReader::~CCSDSReader() {
  close();
}

// open file in binary mode, true if successful
bool CCSDSReader::open() {
  //file.open(filename, std::ios::binary);
  //std::cout << "ERROR: CCSDSREADER::open status is " << file.is_open() << std::endl;
  //return file.is_open();
  return source->open();
}

// close the file if it is open
void CCSDSReader::close() {
  //if (file.is_open()) {
  if (source->isOpen()) {
    //std::cout << "ERROR: CCSDSREADER::close is closing" << std::endl;
    //file.close();
    source->close();
  }
}

// find the sync marker
bool CCSDSReader::findSyncMarker() {

  uint32_t buffer = 0;
  uint8_t onebyte = 0;
  int bytecounter = 0;

  // read one byte at a time shifting 8 bits each iteration - endian-safe
  while (buffer != SYNC_MARKER) {
    if (source->read((&onebyte), sizeof(onebyte))) {
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

// read the sync marker, packet header, and packet data
bool CCSDSReader::readNextPacket(std::vector<uint8_t>& packet) {
  if (!findSyncMarker()) {
    std::cout << "ERROR: CCSDSREADER::readNextPacket did not find sync marker " << std::endl;
    return false;
  }

  std::vector<uint8_t> header(PACKET_HEADER_SIZE);
  if (!readPacketHeader(header)) {
    std::cout << "ERROR: CCSDSREADER::readNextPacket did not read the packet header " << std::endl;
    return false; // failed to read a packet header
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
  packet.resize(PACKET_HEADER_SIZE + packetLength + 1) ; // Allocate space for the packet
  
  // Copy primary header to packet
  std::memcpy(packet.data(), header.data(), PACKET_HEADER_SIZE); 

  // Debugging output: print header in hex
  //std::cout << "Header: ";
  //for (size_t i = 0; i < PACKET_HEADER_SIZE; ++i) {
  //    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(header[i]) << " ";
  //}
  //std::cout << std::endl;

  // Read the packet data
  //if (!source->read(reinterpret_cast<char*>(packet.data() + PACKET_HEADER_SIZE), packetLength + 1)) {
  if (!source->read((packet.data() + PACKET_HEADER_SIZE), packetLength + 1)) {
    std::cout << "ERROR: CCSDSREADER::readNextPacket failed to read data " << std::endl;
    return false; // Failed to read the packet data
  }

  // Debugging output: print payload in hex
  //std::cout << "Payload: ";
  //for (size_t i = PACKET_HEADER_SIZE; i < packet.size(); ++i) {
  //  std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packet[i]) << " ";
  //}
  //std::cout << std::endl;

  return true;
}

// helper method to read the 6-byte CCSDS primary packet header
bool CCSDSReader::readPacketHeader(std::vector<uint8_t>& header) {
  if (source->read((header.data()), PACKET_HEADER_SIZE)) {
    return true; //read success
  } else {
    return false; // read failed (eof or error)
  }
}
// helper method to read the packet data
//bool CCSDSReader::readPacketData(std::vector<uint8_t>& packet) {
//  if (source->read((packet.data()), getPacketLength(packet))) {
//    return true; //read success
//  } else {
//    return false; // read failed (eof or error)
//  }
//}

// helper method to get the packet length from the primary header
uint16_t CCSDSReader::getPacketLength(const std::vector<uint8_t>& header) {
  //CCSDS primary header contains the length excluding primary header
  uint16_t packetLength = (header[4] << 8) | header[5];
  return packetLength;
}

// get the APID from the header
uint16_t CCSDSReader::getAPID(const std::vector<uint8_t>& header) {
  // Least significant 11-bits of first 2 header bytes
  uint16_t apid = ((static_cast<uint16_t>(header[0]) & 0x07) << 8) | static_cast<uint16_t>(header[1]); 
  return apid;
}

// helper method to get the source sequence counter from the primary header
uint16_t CCSDSReader::getSourceSequenceCounter(const std::vector<uint8_t>& header) {
  //CCSDS primary header contains the 14-bits of the source sequence counter
  uint16_t sourceSequenceCounter = ((static_cast<uint16_t>(header[2]) & 0x3F) << 8) | static_cast<uint16_t>(header[3]);
  return sourceSequenceCounter;
}

double CCSDSReader::getPacketTimeStamp(const std::vector<uint8_t>& payload) {

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

uint16_t CCSDSReader::getMode(const std::vector<uint8_t>& payload) {

  uint16_t mode;
  uint32_t offset = 8; // skip 8 bytes from timestamp 

  mode = (static_cast<uint16_t>(payload[offset]<< 8)) | static_cast<uint16_t>(payload[offset+1]);

  return mode;
}

template<typename T>
T CCSDSReader::readValue() {
  T value;
  //source->read(reinterpret_cast<char*>(&value), sizeof(T));
  source->read((&value), sizeof(T));
  return value;
}
