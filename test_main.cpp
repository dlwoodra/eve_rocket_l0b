#define CATCH_CONFIG_MAIN // tell catch to include a main()
#include "catch.hpp" 
//#include <cassert>

//#include "spdlog/spdlog.h" // fancy logging needs a main

#include "CCSDSReader.hpp"
#include "fileutils.hpp"

TEST_CASE("Open valid file") {
  CCSDSReader pktreader("packetizer_out.bin");
  REQUIRE(pktreader.open() == true);
  pktreader.close();
}

TEST_CASE("Open non-existent file") {
  CCSDSReader pktreader("non_existent_file.bin");
  REQUIRE(pktreader.open() == false);
}

TEST_CASE("Find sync marker") {
  // Assume a file "sync_marker_test.bin" contains the SYNC_MARKER in the middle.
  CCSDSReader pktreader("packetizer_out.bin");
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
  CCSDSReader pktreader("packetizer_out.bin");
  REQUIRE(pktreader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(pktreader.readNextPacket(packet) == true);

  // Verify the packet content (length, header, etc.)
  REQUIRE(packet.size() == expected_packet_size);
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
