#include "CCSDSReader.hpp"
#include "commonFunctions.hpp"
#include "eve_l0b.hpp"
//#include "ProgramState.hpp"

//extern ProgramState globalState;

// initialize to use filename
CCSDSReader::CCSDSReader(InputSource* source) : source(source) {}

// close the file if it is open
CCSDSReader::~CCSDSReader() {
  close();
}

// open file in binary mode, true if successful
bool CCSDSReader::open() {
  return source->open();
}

// close the file if it is open
void CCSDSReader::close() {
  if (source->isOpen()) {
    source->close();
  }
}

// find the sync marker
bool CCSDSReader::findSyncMarker() {

  uint32_t buffer = 0;
  uint8_t onebyte = 0;
  uint32_t bytecounter = 0;
  static bool warned = false;

  // read one byte at a time shifting 8 bits each iteration - endian-safe
  while (buffer != SYNC_MARKER) {
    if (source->read((&onebyte), sizeof(onebyte))) {
      buffer = (static_cast<uint32_t>(buffer) << 8) | static_cast<uint32_t>(onebyte);
      bytecounter++;
    } else { 
      return false; // eof
    }
  }
  if ((!warned) && (bytecounter > 4)) {
    std::cout << "Warning - findSyncMarker expected 4 bytes, counted "<< bytecounter << std::endl;
    warned = true;
  }
  return true;
}

// read the sync marker, packet header, and packet data
bool CCSDSReader::readNextPacket(std::vector<uint8_t>& packet) {

  globalState.totalReadCounter.fetch_add(1, std::memory_order_relaxed);

  // slow down for debugging
  //if ( guiEnabled ) {
  //  //During file processing we should pause
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // DELAY, SLOW DOWN FOR TESTING
  //}

  if (!findSyncMarker()) {
    std::cout << "ERROR: CCSDSREADER::readNextPacket did not find sync marker " << std::endl;
    return false;
  }

  //std::cout << "readNextPacket calling readPacketHeader" << std::endl;
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

  if (getAPID(header) == ESP_APID) {
    globalState.packetsPerSecond.store(globalState.totalReadCounter.load()); //totalPacketCounter;    
    globalState.readsPerSecond.store(globalState.totalReadCounter.load());
    globalState.totalReadCounter.store(0, std::memory_order_relaxed);
  }

  // Allocate space for the packet
  packet.resize(PACKET_HEADER_SIZE + packetLength + 1) ; // Allocate space for the packet
  
  // Copy primary header to packet
  std::memcpy(packet.data(), header.data(), PACKET_HEADER_SIZE); 

  // Read the packet data
  if (!source->read((packet.data() + PACKET_HEADER_SIZE), packetLength + 1)) {
    std::cout << "ERROR: CCSDSREADER::readNextPacket failed to read data " << std::endl;
    return false; // Failed to read the packet data
  }

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

  // byte 6 and 7 are allocated, but unused in the rocket fpga
  // only MSB 16-bits (offset+4 and 5) contain subseconds
  timestamp = static_cast<double>(seconds) + 
    (static_cast<double>(subseconds) * (ONE_OVER_65536)); //multiplcation is faster than division

  return timestamp;
}
