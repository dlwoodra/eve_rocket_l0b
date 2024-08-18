#define CATCH_CONFIG_MAIN // tell catch to include a main()
#include "catch.hpp" 

//#include "spdlog/spdlog.h" // fancy logging needs a main

#include "CCSDSReader.hpp"
#include "fileutils.hpp"
#include "RecordFileWriter.hpp"

// Helper class for file cleanup
class TestingFileCleanup {
public:
    explicit TestingFileCleanup(const std::string& filename) : filename(filename) {}
    ~TestingFileCleanup() {
        std::cout << "Deleting test file " << filename.c_str() << std::endl;
        std::remove(filename.c_str());
    }
private:
    std::string filename;
};

// CCSDSReader tests

TEST_CASE("Open valid file") {
  //CCSDSReader pktreader("packetizer_out2.bin");
  CCSDSReader pktreader("packetizer_out_2024_08_13.bin");
  REQUIRE(pktreader.open() == true);
  pktreader.close();
}

TEST_CASE("Open non-existent file") {
  CCSDSReader pktreader("non_existent_file.bin");
  REQUIRE(pktreader.open() == false);
}

TEST_CASE("Find sync marker") {
  // Assume a file "sync_marker_test.bin" contains the SYNC_MARKER in the middle.
  CCSDSReader pktreader("packetizer_out2.bin");
  REQUIRE(pktreader.open() == true);
    
  // Attempt to read the first packet, which includes finding the sync marker
  std::vector<uint8_t> packet;
  bool packetRead = pktreader.readNextPacket(packet);
  INFO("Packet read status: " << packetRead);
  REQUIRE(packetRead == true);

  pktreader.close();
}

TEST_CASE("Read a full packet") {
  const int expected_packet_size = 1768; // fixed packet lengths

  // Create a test file with a known sync marker and packet
  CCSDSReader pktreader("packetizer_out2.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == true);

  // Verify the packet content (length, header, etc.)
  REQUIRE(packet.size() == expected_packet_size);
  uint16_t apid = pktreader.getAPID(packet);
  REQUIRE(apid >= MEGSA_APID); // 601
  REQUIRE(apid <= ESP_APID); // 605
  uint16_t sourceSequenceCounter = pktreader.getSourceSequenceCounter(packet);
  REQUIRE(sourceSequenceCounter >= 0);
  REQUIRE(sourceSequenceCounter < 16384);
  // Additional content checks can be performed here
  pktreader.close();
}

TEST_CASE("Empty file") {
  CCSDSReader pktreader("empty_file.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == false); // No packet should be read
  pktreader.close();
}

TEST_CASE("Corrupted packet header") {
  CCSDSReader pktreader("corrupted_header.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == false); // The packet should fail to read
  pktreader.close();
}

TEST_CASE("EOF before end of packet") {
  CCSDSReader pktreader("incomplete_packet.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == false); // Packet should fail to read
  pktreader.close();
}

TEST_CASE("File without sync marker") {
  CCSDSReader pktreader("no_sync_marker.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == false); // No sync marker found
  pktreader.close();
}


// Tests for RecordFileWriter

// Test case for checking the creation of the RecordFileWriter object and file opening
TEST_CASE("RecordFileWriter creates a file and opens it") {
    RecordFileWriter writer;
    
    REQUIRE(writer.writeSyncAndPacketToRecordFile({0x01, 0x02, 0x03, 0x04}));
    writer.close();
}

// Test case for checking if data is written to the record file
TEST_CASE("RecordFileWriter writes sync marker and packet data") {
    RecordFileWriter writer;

    writer.generateFilename();

    std::vector<uint8_t> testPacket = {0xAA, 0xBB, 0xCC, 0xDD};
    REQUIRE(writer.writeSyncAndPacketToRecordFile(testPacket));

    writer.close();

    // Validate that the file was created and contains expected data
    std::ifstream inputFile(writer.getRecordFilename(), std::ios::binary);
    REQUIRE(inputFile.is_open());

    // Read the sync marker
    uint32_t syncMarker;
    inputFile.read(reinterpret_cast<char*>(&syncMarker), sizeof(syncMarker));
    REQUIRE(syncMarker == SYNC_MARKER);

    // Read the packet data
    std::vector<uint8_t> readPacket(testPacket.size());
    inputFile.read(reinterpret_cast<char*>(readPacket.data()), readPacket.size());

    REQUIRE(readPacket == testPacket);

    // verify that the read pointer has read 8 bytes from the file
    // 4 bytes for the sync marker and 4 for the dummy 4-byte testPacket
    //std::cout << "tellg = " << inputFile.tellg() << std::endl;
    REQUIRE(inputFile.tellg() == 8); // Check file size is after byte 8

}

// Test case for closing the file
TEST_CASE("RecordFileWriter closes the file properly") {
    RecordFileWriter writer;
    std::string filename = writer.getRecordFilename();

    writer.close();

    REQUIRE_FALSE(writer.writeSyncAndPacketToRecordFile({0x01, 0x02, 0x03, 0x04}));

    //std::remove(filename.c_str()); // Delete the file
}

TEST_CASE("RecordFileWriter opens and writes to the record file") {
    RecordFileWriter writer;
    std::string filename = writer.getRecordFilename();

    REQUIRE(!filename.empty());

    std::vector<uint8_t> packet = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(writer.writeSyncAndPacketToRecordFile(packet));

    //// Ensure the file exists and is non-empty
    //std::ifstream infile(filename, std::ios::binary | std::ios::ate);
    //REQUIRE(infile.is_open());
    //REQUIRE(infile.tellg() > 0); // Check file size is greater than 0

    //Destructor deals with file cleanup
    TestingFileCleanup cleanup(filename);

    //// Cleanup: Delete the temporary file
    //infile.close(); // Close the file before deleting
    //std::remove(filename.c_str()); // Delete the file
}

/*

int test_main( int argc, char* argv[] ) {

  Catch::Session session; // only one instance

  // writing to session.configData() here sets defaults
  // this is the preferred way to set them

  int returnCode = session.applyCommandLine( argc, argv );
  if( returnCode != 0 ) // Indicates a command line error
        return returnCode;

  // writing to session.configData() or session.Config() here
  // overrides command line args
  // only do this if you know you need to

  int numFailed = session.run();

  // numFailed is clamped to 255 as some unices only use the lower 8 bits.
  // This clamping has already been applied, so just return it here
  // You can also do any post run clean-up here
  return numFailed;
}

*/
