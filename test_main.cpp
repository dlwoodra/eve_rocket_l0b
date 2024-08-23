#define CATCH_CONFIG_MAIN // tell catch to include a main()
#include "catch.hpp" 

//#include "spdlog/spdlog.h" // fancy logging needs a main

#include "CCSDSReader.hpp"
#include "fileutils.hpp"
#include "RecordFileWriter.hpp"
#include "TimeInfo.hpp"
#include "InputSource.hpp"
#include "FileInputSource.hpp"

//#define byteswap_32(x) \
//    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |		      \
//    (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))


// Helper function to get current year using std::chrono for testing TimeInfo
int getCurrentYear() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    gmtime_r(&now_c, &now_tm);
    return now_tm.tm_year + 1900;
}


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
  FileInputSource fileSource("packetizer_out_2024_08_20.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);
  fileReader.close();
}

TEST_CASE("Open non-existent file") {
  FileInputSource fileSource("non_existent_file.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == false);
}

TEST_CASE("Find sync marker") {
  // Assume a file "sync_marker_test.bin" contains the SYNC_MARKER in the middle.
  FileInputSource fileSource("packetizer_out2.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);
    
  // Attempt to read the first packet, which includes finding the sync marker
  std::vector<uint8_t> packet;
  bool packetRead = fileReader.readNextPacket(packet);
  INFO("Packet read status: " << packetRead);
  REQUIRE(packetRead == true);

  fileReader.close();
}

TEST_CASE("Read a full packet") {
  const int expected_packet_size = 1768; // fixed packet lengths

  // Create a test file with a known sync marker and packet
  FileInputSource fileSource("packetizer_out2.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == true);

  // Verify the packet content (length, header, etc.)
  REQUIRE(packet.size() == expected_packet_size);
  uint16_t apid = fileReader.getAPID(packet);
  REQUIRE(apid >= MEGSA_APID); // 601
  REQUIRE(apid <= ESP_APID); // 605
  uint16_t sourceSequenceCounter = fileReader.getSourceSequenceCounter(packet);
  REQUIRE(sourceSequenceCounter >= 0);
  REQUIRE(sourceSequenceCounter < 16384);
  // Additional content checks can be performed here
  fileReader.close();
}

TEST_CASE("Empty file") {
  FileInputSource fileSource("empty_file.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == false); // No packet should be read
  fileReader.close();
}

TEST_CASE("Corrupted packet header") {
  FileInputSource fileSource("corrupted_header.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == false); // The packet should fail to read
  fileReader.close();
}

TEST_CASE("EOF before end of packet") {
  FileInputSource fileSource("incomplete_packet.bin");
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == false); // Packet should fail to read
  fileReader.close();
}

TEST_CASE("File without sync marker") {
  FileInputSource fileSource("no_sync_marker.bin");
  //CCSDSReader pktReader(filename);
  CCSDSReader fileReader(&fileSource);
  //CCSDSReader pktReader("no_sync_marker.bin");
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == false); // No sync marker found
  fileReader.close();
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

    writer.generateRecordFilename();

    std::vector<uint8_t> testPacket = {0xAA, 0xBB, 0xCC, 0xDD};
    REQUIRE(writer.writeSyncAndPacketToRecordFile(testPacket));

    writer.close();

    // Validate that the file was created and contains expected data
    std::ifstream inputFile(writer.getRecordFilename(), std::ios::binary);
    REQUIRE(inputFile.is_open());

    // Read the sync marker
    uint32_t syncMarker;
    uint8_t syncMarkerBytes[4];
    inputFile.read(reinterpret_cast<char*>(&syncMarkerBytes[0]), sizeof(syncMarkerBytes[0]));
    inputFile.read(reinterpret_cast<char*>(&syncMarkerBytes[1]), sizeof(syncMarkerBytes[1]));
    inputFile.read(reinterpret_cast<char*>(&syncMarkerBytes[2]), sizeof(syncMarkerBytes[2]));
    inputFile.read(reinterpret_cast<char*>(&syncMarkerBytes[3]), sizeof(syncMarkerBytes[3]));
    syncMarker = ( uint32_t (syncMarkerBytes[0] << 24) ) | \
      ( uint32_t (syncMarkerBytes[1] << 16) ) | \
      ( uint32_t (syncMarkerBytes[2] << 8 ) | \
      ( uint32_t (syncMarkerBytes[3]) ) );
    REQUIRE((syncMarker) == SYNC_MARKER);

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

    // this should fail since the file is closed
    bool writeResult = writer.writeSyncAndPacketToRecordFile({0x01, 0x02, 0x03, 0x04});
    REQUIRE_FALSE(writeResult);

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

TEST_CASE("TimeInfo converts properly") {
    TimeInfo currentTime;

    std::cout << "Year: " << currentTime.getYear() << "\n";
    std::cout << "Day of Year: " << currentTime.getDayOfYear() << "\n";
    std::cout << "Month: " << currentTime.getMonth() << "\n";
    std::cout << "Day of Month: " << currentTime.getDayOfMonth() << "\n";
    std::cout << "Hour: " << currentTime.getHour() << "\n";
    std::cout << "Minute: " << currentTime.getMinute() << "\n";
    std::cout << "Second: " << currentTime.getSecond() << "\n";
    std::cout << "Microseconds since Epoch: " << currentTime.getMicrosecondsSinceEpoch() << "\n";
    std::cout << "UTC Subseconds: " << currentTime.getUTCSubseconds() << "\n";
    std::cout << "TAI Seconds: " << currentTime.getTAISeconds() << "\n";
    std::cout << "TAI Subseconds: " << currentTime.getTAISubseconds() << "\n";

}

// Test suite for the TimeInfo class
TEST_CASE("TimeInfo class functionality", "[TimeInfo]") {
    TimeInfo timeInfo;

    SECTION("UpdateNow should update time components") {
        timeInfo.updateNow();
        REQUIRE(timeInfo.getYear() == getCurrentYear());
    }

    SECTION("Check Year") {
        REQUIRE(timeInfo.getYear() == getCurrentYear());
    }

    SECTION("Check Day of Year is within valid range") {
        REQUIRE(timeInfo.getDayOfYear() >= 1);
        REQUIRE(timeInfo.getDayOfYear() <= 366);  // Account for leap years
    }

    SECTION("Check Month is within valid range") {
        REQUIRE(timeInfo.getMonth() >= 1);
        REQUIRE(timeInfo.getMonth() <= 12);
    }

    SECTION("Check Day of Month is within valid range") {
        REQUIRE(timeInfo.getDayOfMonth() >= 1);
        REQUIRE(timeInfo.getDayOfMonth() <= 31);
    }

    SECTION("Check Hour is within valid range") {
        REQUIRE(timeInfo.getHour() >= 0);
        REQUIRE(timeInfo.getHour() <= 23);
    }

    SECTION("Check Minute is within valid range") {
        REQUIRE(timeInfo.getMinute() >= 0);
        REQUIRE(timeInfo.getMinute() <= 59);
    }

    SECTION("Check Second is within valid range") {
        REQUIRE(timeInfo.getSecond() >= 0);
        REQUIRE(timeInfo.getSecond() <= 60);  // Account for leap seconds
    }

    SECTION("Check Microseconds Since Epoch") {
        double microseconds = timeInfo.getMicrosecondsSinceEpoch();
        REQUIRE(microseconds > 0);
    }

    SECTION("Check UTC Subseconds") {
        double subseconds = timeInfo.getUTCSubseconds();
        REQUIRE(subseconds >= 0.0);
        REQUIRE(subseconds < 1.0);
    }

    SECTION("Check TAI Seconds") {
        double taiSeconds = timeInfo.getTAISeconds();
        double expectedTAI = (timeInfo.getMicrosecondsSinceEpoch() / MICROSECONDS_PER_SECOND) +
                              TAI_LEAP_SECONDS + TAI_EPOCH_OFFSET_TO_UNIX;
        REQUIRE(taiSeconds == Approx(expectedTAI));
    }

    SECTION("Check TAI Subseconds") {
        double taiSubseconds = timeInfo.getTAISubseconds();
        REQUIRE(taiSubseconds == Approx(timeInfo.getUTCSubseconds()));
    }
}

// Test suite for the isValidFilename function
TEST_CASE("isValidFilename function tests", "[isValidFilename]") {

    SECTION("Returns false for empty string") {
        REQUIRE(isValidFilename("") == false);
    }

    SECTION("Returns true for non-empty string") {
        REQUIRE(isValidFilename("example.txt") == true);
        REQUIRE(isValidFilename("filename_with_underscores") == true);
        REQUIRE(isValidFilename("1234567890") == true);
        REQUIRE(isValidFilename(".hiddenfile") == true);
        REQUIRE(isValidFilename("file with spaces.txt") == true);
    }

    SECTION("Returns true for single character string") {
        REQUIRE(isValidFilename("a") == true);
    }

    SECTION("Returns true for special characters in filename") {
        REQUIRE(isValidFilename("file-name_with.mixed-characters_123") == true);
    }
}
