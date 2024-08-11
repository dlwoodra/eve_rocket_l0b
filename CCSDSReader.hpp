#ifndef CCSDS_READER_HPP
#define CCSDS_READER_HPP

#include <fstream>
#include <vector>
#include <cstdint>

// Constants
const uint32_t SYNC_MARKER = 0x1ACFFC1D; // Example sync marker, typical for CCSDS frames
const size_t PACKET_HEADER_SIZE = 6;     // CCSDS packet primary header size in bytes
const uint16_t STANDARD_PACKET_LENGTH = 1761; // value from packet (one less than size)
// David's definitions of APIDs
const uint16_t MEGSA_APID = 601; // same as SDO EVE
const uint16_t MEGSB_APID = 602; // same as SDO EVE
const uint16_t MEGSP_APID = 604; // was SHK in SDO EVE
const uint16_t ESP_APID = 605; // not used in SDO EVE, MEGS-P and ESP were both in 603
const uint8_t mask6bit = 0x3f; // upper byte of source sequence counter

class CCSDSReader {
public:
    CCSDSReader(const std::string& filename);
    ~CCSDSReader();

    bool open();       // Open the binary file
    void close();      // Close the binary file
    bool readNextPacket(std::vector<uint8_t>& packet); // Read the next CCSDS packet

    uint16_t getSourceSequenceCounter(const std::vector<uint8_t>& header);
    uint16_t getAPID(const std::vector<uint8_t>& header);

    double getPacketTimeStamp(const std::vector<uint8_t>& payload);

private:
    std::ifstream file;
    std::string filename;

    bool findSyncMarker();     // Locate the sync marker in the binary file
    bool readPacketHeader(std::vector<uint8_t>& header); // Read CCSDS packet header
    bool readPacketData(std::vector<uint8_t>& payload); // Read CCSDS packet header
    uint16_t getPacketLength(const std::vector<uint8_t>& header); // Calculate packet length

    template<typename T>
    T readValue(); // Helper function to read a value of type T from the file
};

#endif // CCSDS_READER_HPP
