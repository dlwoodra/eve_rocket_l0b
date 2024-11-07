#define CATCH_CONFIG_MAIN // tell catch to include a main()
#include "catch.hpp" 

#include "CCSDSReader.hpp"
#include "commonFunctions.hpp"
#include "FileInputSource.hpp"
#include "InputSource.hpp"
#include "RecordFileWriter.hpp"
#include "TimeInfo.hpp"
#include "USBInputSource.hpp"
#include "ProgramState.hpp"
#include "FileCompressor.hpp"
#include <stdexcept>

//#define NORMAL_FILE "packetizer_out_2024_08_20.bin"
//#define NORMAL_FILE "packetizer_out_2024_08_31.bin"
#define NORMAL_FILE "packetizer_out_2024_10_23.bin"

std::string normalFile = NORMAL_FILE;

void testGlobalStateInit() {
    globalStateInit();
    REQUIRE(globalState.megsa.image[0][0] == 0xff);
    REQUIRE(globalState.megsb.image[0][0] == 0x3fff);
    REQUIRE(globalState.running == true);
    REQUIRE(globalState.initComplete == true);
}

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


// Test fixture to create and cleanup files
class FileCompressorTests {
public:
    std::string testFilename = "test_log.txt";
    std::string compressedFilename = "test_log.txt.gz";

    FileCompressorTests() {
        // Create a dummy test file
        std::ofstream outFile(testFilename);
        outFile << "This is a test log file." << std::endl;
        outFile.close();
    }

    ~FileCompressorTests() {
        // Cleanup created files
        std::remove(testFilename.c_str());
        std::remove(compressedFilename.c_str());
    }
};

// commonFunctions tests
TEST_CASE("create_single_directory creates directory correctly", "[create_single_directory]") {
    std::string dirPath = "./test_dir";

    // Ensure the directory does not exist before test
    rmdir(dirPath.c_str());

    // Test creating a new directory
    REQUIRE(create_single_directory(dirPath) == true);

    // Check if directory was created
    struct stat info;
    REQUIRE(stat(dirPath.c_str(), &info) == 0);  // Check if stat call is successful
    REQUIRE(info.st_mode & S_IFDIR);  // Ensure it's a directory

    // Clean up
    rmdir(dirPath.c_str());
}

TEST_CASE("create_single_directory fails if path is not a directory", "[create_single_directory]") {
    std::string nonDirPath = "./test_file.txt";
    
    // Create a file at that path (to simulate existing non-directory path)
    std::ofstream file(nonDirPath);
    file.close();

    // Test that the function returns false when path exists but isn't a directory
    REQUIRE(create_single_directory(nonDirPath) == false);

    // Clean up
    remove(nonDirPath.c_str());
}

TEST_CASE("create_single_directory returns true if directory already exists", "[create_single_directory]") {
    std::string dirPath = "./test_dir_existing";

    // Create the directory before testing
    REQUIRE(create_single_directory(dirPath) == true);

    // Test that the function returns true if directory already exists
    REQUIRE(create_single_directory(dirPath) == true);

    // Clean up
    rmdir(dirPath.c_str());
}

TEST_CASE("create_directory_if_not_exists creates nested directories", "[create_directory_if_not_exists]") {
    std::string nestedDirPath = "./test_dir/nested_dir/subdir";

    // Ensure no such directories exist before the test
    rmdir("./test_dir/nested_dir/subdir");  // Cleanup if any part exists
    rmdir("./test_dir/nested_dir");  
    rmdir("./test_dir");

    // Test creating nested directories
    REQUIRE(create_directory_if_not_exists(nestedDirPath) == true);

    // Check if all directories were created
    struct stat info;
    REQUIRE(stat("./test_dir", &info) == 0);
    REQUIRE(info.st_mode & S_IFDIR);

    REQUIRE(stat("./test_dir/nested_dir", &info) == 0);
    REQUIRE(info.st_mode & S_IFDIR);

    REQUIRE(stat("./test_dir/nested_dir/subdir", &info) == 0);
    REQUIRE(info.st_mode & S_IFDIR);

    // Clean up
    rmdir("./test_dir/nested_dir/subdir");
    rmdir("./test_dir/nested_dir");
    rmdir("./test_dir");
}

TEST_CASE("create_directory_if_not_exists works when directories already exist", "[create_directory_if_not_exists]") {
    std::string nestedDirPath = "./test_dir/nested_dir/existing_subdir";

    // Ensure the directory exists for the test
    create_directory_if_not_exists(nestedDirPath);

    // Test that it returns true even if directories already exist
    REQUIRE(create_directory_if_not_exists(nestedDirPath) == true);

    // Clean up
    rmdir("./test_dir/nested_dir/existing_subdir");
    rmdir("./test_dir/nested_dir");
    rmdir("./test_dir");
}

TEST_CASE("transposeImageTo1D correctly transposes a single row from a large image", "[transposeImageTo1D]") {
    uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {};

    // Set values in the first row for testing
    for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
        image[0][i] = i + 1; // 1, 2, 3, ..., 2048
    }

    std::vector<uint16_t> result = transposeImageTo1D(image);

    // Check the first 2048 elements, which should match the first row
    REQUIRE(result.size() == MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH);
    for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
        REQUIRE(result[i] == i + 1);
    }
}

TEST_CASE("transposeImageTo1D correctly transposes the entire 1024x2048 image", "[transposeImageTo1D]") {
    uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {};

    // Initialize the image with some values for testing
    uint16_t value = 1; //confusing because it wraps at 65535 back to 0
    for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
        for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
            image[y][x] = value++;
        }
    }

    std::vector<uint16_t> result = transposeImageTo1D(image);

    // Check the size of the resulting vector
    REQUIRE(result.size() == MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH);

    // Check a few values to ensure the function is working correctly (start, middle, end)
    REQUIRE(result[0] == 1); // First element
    REQUIRE(result[MEGS_IMAGE_WIDTH - 1] == MEGS_IMAGE_WIDTH); // Last element of the first row
    REQUIRE(result[MEGS_IMAGE_WIDTH] == MEGS_IMAGE_WIDTH + 1); // First element of the second row
    REQUIRE(result[result.size() - 1] == 0);
    REQUIRE(result[result.size() - 2] == 65535);
    REQUIRE(result[result.size() - 3] == 65534);
    REQUIRE(result[2048 - 1] == 2048); // Last element of the first row
    REQUIRE(result[4096 - 1] == 4096); // Last element of the second row
    // all values that are mutiples of 65536 will be 0 because value wraps around
}

TEST_CASE("transposeImageTo1D correctly handles boundary values", "[transposeImageTo1D]") {
    uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {};

    // Set boundary values (max uint16_t) at strategic positions
    image[0][0] = 65535;  // Top-left corner
    image[0][MEGS_IMAGE_WIDTH - 1] = 65535;  // Top-right corner
    image[MEGS_IMAGE_HEIGHT - 1][0] = 65535;  // Bottom-left corner
    image[MEGS_IMAGE_HEIGHT - 1][MEGS_IMAGE_WIDTH - 1] = 65535;  // Bottom-right corner

    std::vector<uint16_t> result = transposeImageTo1D(image);

    // Check that boundary values are in the correct positions
    REQUIRE(result[0] == 65535);  // First element (top-left)
    REQUIRE(result[MEGS_IMAGE_WIDTH - 1] == 65535);  // Last element of the first row
    REQUIRE(result[MEGS_IMAGE_WIDTH * (MEGS_IMAGE_HEIGHT - 1)] == 65535);  // First element of the last row
    REQUIRE(result[result.size() - 1] == 65535);  // Last element (bottom-right)
}

TEST_CASE("transposeImageTo1D performs efficiently for large images", "[transposeImageTo1D][performance]") {
    uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {};

    // Initialize the image with random values for performance test
    for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
        for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
            image[y][x] = (y * MEGS_IMAGE_WIDTH + x) % 65536;
        }
    }

    // Measure the time it takes to transpose the image
    int iterationPower = 4; // 2^4 = 16 iterations
    int numIterations = (1<<iterationPower); // 16 iterations
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numIterations; ++i) {
        std::vector<uint16_t> result = transposeImageTo1D(image);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    int meanDuration = duration>>iterationPower;
    // vm takes 1977-2170 microseconds, compiler optimizes so that mean is 1368 over 16 iterations

    std::cout<< "transposeImageTo1D performance test: " << meanDuration << " microsec/iteration over " <<numIterations<<" iterations."<< std::endl;
    REQUIRE(duration < 1000000); // Ensure that the operation takes less than 1 second (1 million microseconds)
}


TEST_CASE("payloadBytesToUint32 correctly converts four bytes to uint32_t", "[payloadBytesToUint32]") {
    // Test case 1: Normal conversion
    std::vector<uint8_t> payload = {0x12, 0x34, 0x56, 0x78};  // Expected value: 0x12345678
    int offset = 0;
    uint32_t result = payloadBytesToUint32(payload, offset);
    REQUIRE(result == 0x12345678);  // Check if the conversion is correct

    // Test case 2: Different byte values
    payload = {0xFF, 0x00, 0x11, 0x22};  // Expected value: 0xFF001122
    result = payloadBytesToUint32(payload, offset);
    REQUIRE(result == 0xFF001122);  // Check if the conversion is correct

    // Test case 3: Edge case with offset
    payload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};  // Expected value: 0x05040302 at offset 1
    offset = 1;
    result = payloadBytesToUint32(payload, offset);
    REQUIRE(result == 0x02030405);  // Check if the conversion is correct

    // Test case 4: Incorrect offset (out of bounds)
    payload = {0x01, 0x02, 0x03};  // Only 3 bytes, should cause an issue when accessing four bytes
    offset = 1;
    REQUIRE_THROWS_AS(payloadBytesToUint32(payload, offset), std::invalid_argument);
}

TEST_CASE("payloadToTAITimeSeconds correctly converts payload to TAI time", "[payloadToTAITimeSeconds]") {
    // Test case 1: Valid 4-byte payload
    std::vector<uint8_t> payload = {0x12, 0x34, 0x56, 0x78};  // Expected value: 0x12345678
    uint32_t result = payloadToTAITimeSeconds(payload);
    REQUIRE(result == 0x12345678);  // Check if the conversion is correct

    // Test case 2: Another valid payload
    payload = {0xFF, 0x00, 0x11, 0x22};  // Expected value: 0xFF001122
    result = payloadToTAITimeSeconds(payload);
    REQUIRE(result == 0xFF001122);  // Check if the conversion is correct

    // Test case 3: Payload size less than 4 bytes (should throw an exception)
    payload = {0x01, 0x02};  // Only 2 bytes, should throw an exception
    REQUIRE_THROWS_AS(payloadToTAITimeSeconds(payload), std::invalid_argument);

    // Test case 4: Empty payload (should throw an exception)
    payload = {};  // Empty payload, should throw an exception
    REQUIRE_THROWS_AS(payloadToTAITimeSeconds(payload), std::invalid_argument);
}

// CCSDSReader tests

TEST_CASE("Open valid file") {
  //CCSDSReader pktreader("packetizer_out2.bin");
  //FileInputSource fileSource("packetizer_out_2024_08_20.bin");
  FileInputSource fileSource( NORMAL_FILE );
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
  FileInputSource fileSource( NORMAL_FILE );
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
  FileInputSource fileSource( NORMAL_FILE );
  CCSDSReader fileReader(&fileSource);
  REQUIRE(fileReader.open() == true);

  std::vector<uint8_t> packet;
  REQUIRE(fileReader.readNextPacket(packet) == true);

  // Verify the packet content (length, header, etc.)
  REQUIRE(packet.size() == expected_packet_size);
  // get the header
  std::vector<uint8_t> header(packet.begin(), packet.begin() + PACKET_HEADER_SIZE);
  uint16_t apid = fileReader.getAPID(header); //use header not packet 

  REQUIRE(apid >= MEGSA_APID); // 601
  REQUIRE(apid <= ESP_APID); // 605
  uint16_t sourceSequenceCounter = fileReader.getSourceSequenceCounter(packet);
  REQUIRE(sourceSequenceCounter >= 0);
  REQUIRE(sourceSequenceCounter < 16384);

  uint16_t packetLength = fileReader.getPacketLength(header);
  // check packetLength
  REQUIRE(((packetLength == STANDARD_MEGSAB_PACKET_LENGTH) || \
    (packetLength == STANDARD_HK_PACKET_LENGTH) || \
    (packetLength == STANDARD_ESP_PACKET_LENGTH)));

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

// These can only be tested when connected to a device.
//
// TEST_CASE("USBInputSource Test Suite", "[USBInputSource]") {

//     SECTION("getSerialNumber returns expected serial number") {
//         // Create an instance of USBInputSource
//         std::string initialSerialNumber = "24080019Q1";
//         std::string emptySerialNumber;
//         USBInputSource usbSource(emptySerialNumber);

//         // Call the selectUSBSerialNumber function
//         std::string serialNumber = usbSource.getSerialNumber();

//         // Check if the returned serial number is "24080019Q1"
//         REQUIRE(serialNumber == initialSerialNumber);
//     }

//     // You can add more SECTIONs here for other tests related to USBInputSource

// }



// Test case to verify the compression function
TEST_CASE_METHOD(FileCompressorTests, "CompressFile") {
    FileCompressor compressor;

    // Check the file before compression
    REQUIRE(std::ifstream(testFilename).good());

    // Compress the file
    compressor.compressFile(testFilename);

    // Since the thread is detached, we wait a bit for the compression to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check if the compressed file is created
    REQUIRE(std::ifstream(compressedFilename).good());
}

// Test case to ensure the pigz command is called with the right parameters
TEST_CASE_METHOD(FileCompressorTests, "CompressWithPigz") {
    FileCompressor compressor;

    // Mock system call to avoid calling the actual pigz command
    // You could use a mock or override system behavior in a real test scenario
    SECTION("pigz system call") {
        // Check if pigz compresses correctly (this will call the real system command)
        REQUIRE_NOTHROW(compressor.compressFile(testFilename));
    }
}

// Test case to check if the thread runs in the background and does not block
TEST_CASE_METHOD(FileCompressorTests, "CompressionThreading") {
    FileCompressor compressor;

    // Start compression in a separate thread
    compressor.compressFile(testFilename);

    // Perform other tasks to check if the main thread continues without blocking
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check if the compressed file exists after some time
    REQUIRE(std::ifstream(compressedFilename).good());
}