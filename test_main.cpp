#define CATCH_CONFIG_MAIN // tell catch to include a main()
//#include "catch.hpp" 
#include <catch2/catch.hpp>

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
#include <omp.h>

//#define NORMAL_FILE "packetizer_out_2024_08_20.bin"
//#define NORMAL_FILE "packetizer_out_2024_08_31.bin"
#define NORMAL_FILE "packetizer_out_2024_10_23.bin"

std::string normalFile = NORMAL_FILE;

void testGlobalStateInit() {
    globalStateInit();
    REQUIRE(globalState.megsa.image[0][0] == 0xff);
    REQUIRE(globalState.megsb.image[0][0] == 0x3fff);
    REQUIRE(globalState.running.load(std::memory_order_relaxed) == true);
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

// Test the toISO8601 function
TEST_CASE("toISO8601 converts date and time components to correct ISO 8601 format", "[toISO8601]") {

    // Test with a regular date
    SECTION("Test standard date") {
        int year = 2024, dayOfYear = 120, hour = 10, minute = 30, second = 45;
        std::string iso8601 = toISO8601(year, dayOfYear, hour, minute, second);
        REQUIRE(iso8601 == "2024-04-29T10:30:45");  // April 29th, 2024, 10:30:45
    }

    // Test with a leap year date
    SECTION("Test leap year") {
        int year = 2020, dayOfYear = 60, hour = 23, minute = 59, second = 59;
        std::string iso8601 = toISO8601(year, dayOfYear, hour, minute, second);
        REQUIRE(iso8601 == "2020-02-29T23:59:59");  // February 29th, 2020 (leap year)
    }

    // Test with edge of year (December 31st)
    SECTION("Test end of year") {
        int year = 2024, dayOfYear = 365, hour = 23, minute = 59, second = 59;
        std::string iso8601 = toISO8601(year, dayOfYear, hour, minute, second);
        REQUIRE(iso8601 == "2024-12-30T23:59:59");  // December 30th, 2024
    }

    // Test with the beginning of the year (January 1st)
    SECTION("Test beginning of year") {
        int year = 2024, dayOfYear = 1, hour = 0, minute = 0, second = 0;
        std::string iso8601 = toISO8601(year, dayOfYear, hour, minute, second);
        REQUIRE(iso8601 == "2024-01-01T00:00:00");  // January 1st, 2024
    }

    // Test for an invalid day (out of range) to see if the function handles it
    SECTION("Test invalid day") {
        int year = 2024, dayOfYear = 366, hour = 12, minute = 0, second = 0;
        std::string iso8601 = toISO8601(year, dayOfYear, hour, minute, second);
        // This should be an invalid day for 2024, which only has 365 days
        REQUIRE(iso8601 == "2024-12-31T12:00:00"); // Output may vary based on implementation
    }
}

TEST_CASE("get_leap_seconds returns correct leap seconds information", "[get_leap_seconds]") {
    
    uint32_t tai_local = 0;  // Example input, actual value not important for this test
    uint32_t leap_sec_local;

    SECTION("Test getting leap seconds") {
        // Test that the function returns the correct value for leap seconds
        int result = get_leap_seconds(tai_local, &leap_sec_local);
        REQUIRE(result == 0);  // Ensure the function returns 0
        REQUIRE(leap_sec_local == TAI_LEAP_SECONDS);  // Ensure the leap seconds value is returned correctly
    }
}

// countSaturatedPixels tests
TEST_CASE("countSaturatedPixels - General Test Cases") {

    uint32_t saturatedPixelsTop = 0;
    uint32_t saturatedPixelsBottom = 0;

    SECTION("All pixels are non-saturated") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);
        REQUIRE(saturatedPixelsTop == 0);
        REQUIRE(saturatedPixelsBottom == 0);
    }

    SECTION("All pixels are saturated") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH];
        std::fill(&image[0][0], &image[0][0] + MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH, 0x3fff);
        
        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);
        
        REQUIRE(saturatedPixelsTop == (MEGS_IMAGE_HEIGHT / 2) * MEGS_IMAGE_WIDTH);
        REQUIRE(saturatedPixelsBottom == (MEGS_IMAGE_HEIGHT / 2) * MEGS_IMAGE_WIDTH);
    }

    SECTION("Half of the pixels are saturated in top half") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
            for (uint32_t j = 0; j < MEGS_IMAGE_HEIGHT / 2; ++j) {
                image[j][i] = 0x3fff;
            }
        }

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);

        REQUIRE(saturatedPixelsTop == (MEGS_IMAGE_HEIGHT / 2) * MEGS_IMAGE_WIDTH);
        REQUIRE(saturatedPixelsBottom == 0);
    }

    SECTION("Saturated pixels only in bottom half") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
            for (uint32_t j = MEGS_IMAGE_HEIGHT / 2; j < MEGS_IMAGE_HEIGHT; ++j) {
                image[j][i] = 0x3fff;
            }
        }

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);

        REQUIRE(saturatedPixelsTop == 0);
        REQUIRE(saturatedPixelsBottom == (MEGS_IMAGE_HEIGHT / 2) * MEGS_IMAGE_WIDTH);
    }

    SECTION("Single saturated pixel in top half") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        image[10][10] = 0x3fff;

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);

        REQUIRE(saturatedPixelsTop == 1);
        REQUIRE(saturatedPixelsBottom == 0);
    }

    SECTION("Single saturated pixel in bottom half") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        image[MEGS_IMAGE_HEIGHT - 10][10] = 0x3fff;

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);

        REQUIRE(saturatedPixelsTop == 0);
        REQUIRE(saturatedPixelsBottom == 1);
    }

    SECTION("Test pattern with single saturated pixel at the start in top half") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        image[0][0] = 0x3fff;

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom, true);

        REQUIRE(saturatedPixelsTop == 0);  // Should be skipped due to testPattern flag
        REQUIRE(saturatedPixelsBottom == 0);
    }
    
    SECTION("Test pattern with a mix of saturated and non-saturated pixels") {
        uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
        for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
            image[0][i] = 0x3fff;  // Saturated in the top row
            image[MEGS_IMAGE_HEIGHT / 2][i] = 0x3fff;  // Saturated in the bottom half row
        }

        countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom, true);

        REQUIRE(saturatedPixelsTop == MEGS_IMAGE_WIDTH - 1);  // Skip the first pixel
        REQUIRE(saturatedPixelsBottom == MEGS_IMAGE_WIDTH);
    }
}

void populateTestImage(uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], uint16_t saturationValue = 0x3FFF) {
    for (uint32_t i = 0; i < MEGS_IMAGE_HEIGHT; ++i) {
        for (uint32_t j = 0; j < MEGS_IMAGE_WIDTH; ++j) {
            image[i][j] = (i + j) % 2 == 0 ? saturationValue : 0x0000; // Example pattern for testing
        }
    }
}

// Timing utility function
double measureCountSaturatedPixelsExecutionTime(int numThreads, uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH],
                            uint32_t& saturatedPixelsTop, uint32_t& saturatedPixelsBottom) {
    omp_set_num_threads(numThreads);  // Set the number of threads
    auto start = std::chrono::high_resolution_clock::now();
    countSaturatedPixels(image, saturatedPixelsTop, saturatedPixelsBottom);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

TEST_CASE("Performance test for countSaturatedPixels", "[performance]") {
    uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH];
    uint32_t saturatedPixelsTop = 0;
    uint32_t saturatedPixelsBottom = 0;

    populateTestImage(image); // Populate image with test pattern

    SECTION("Single-threaded performance") {
        double timeTaken = measureCountSaturatedPixelsExecutionTime(1, image, saturatedPixelsTop, saturatedPixelsBottom);
        REQUIRE(saturatedPixelsTop > 0);
        REQUIRE(saturatedPixelsBottom > 0);
        std::cout << "countSaturatePixels Single-threaded time: " << timeTaken << " seconds" << std::endl;

    }

    SECTION("Multi-threaded performance with OpenMP") {
        for (int numThreads = 2; numThreads <= 16; numThreads *= 2) {
            saturatedPixelsTop = 0;
            saturatedPixelsBottom = 0;
            double timeTaken = measureCountSaturatedPixelsExecutionTime(numThreads, image, saturatedPixelsTop, saturatedPixelsBottom);
            REQUIRE(saturatedPixelsTop > 0);
            REQUIRE(saturatedPixelsBottom > 0);
            std::cout << "countPixelsSaturates Time with " << numThreads << " threads: " << timeTaken << " seconds" << std::endl;
        }
    }
}


// assemble_image tests
// Helper function to populate a VCDU with a specific pattern
void populateVCDU(uint8_t *vcdu, int size, uint16_t pattern) {
    for (int i = 30; i < size; i += 2) { // pixels start at 30 bytes
        vcdu[i] = (pattern >> 8) & 0xFF;
        vcdu[i + 1] = pattern & 0xFF;
    }
}

TEST_CASE("Basic Functionality of assemble_image", "[assemble_image]") {
    uint8_t vcdu[1781] = {0}; // Test VCDU buffer (size based on max offset used in function)
    MEGS_IMAGE_REC imageRec;   // Struct to hold image data and VCDU counter
    std::memset(imageRec.image, 0, sizeof(imageRec.image));
    int32_t xpos, ypos;
    int8_t status = NOERROR;
    imageRec.vcdu_count = 0; // init

    // Case 1: Basic functionality, no test pattern, expect parity errors to be 0
    populateVCDU(vcdu, sizeof(vcdu), 0x000b); // Populate VCDU with all 0x000b test values
    int32_t parityErrors = assemble_image(vcdu, &imageRec, 0, false, xpos, ypos, &status);

    REQUIRE(parityErrors == 0);                // No parity errors
    REQUIRE(status == NOERROR);                // Status should be NOERROR
    REQUIRE(imageRec.vcdu_count == 1);         // VCDU count should be incremented
    REQUIRE(imageRec.image[0][0] == 0x200b);   // First pixel should have the 2s comp of the test value

    // Check a specific pixel
    REQUIRE(imageRec.image[0][10] == 0x200b); // Arbitrary pixel in first VCDU range should have the expected value
    REQUIRE(imageRec.image[0][433] == 0x200b); // Arbitrary pixel in first VCDU range should have the expected value
    REQUIRE(imageRec.image[0][434] == 0x0000); // Pixel NOT in first VCDU range should be 0
    REQUIRE(imageRec.image[1023][2048-435] == 0x0000); // Pixel NOT in first VCDU range should be 0
    REQUIRE(imageRec.image[1023][2048-434] == 0x200b); // Pixel IS in first VCDU range should have the expected value

}

TEST_CASE("Test Pattern Mode", "[assemble_image]") {
    uint8_t vcdu[1781] = {0};
    MEGS_IMAGE_REC imageRec;
    std::memset(imageRec.image, 0, sizeof(imageRec.image));
    int32_t xpos, ypos;
    int8_t status = NOERROR;
    imageRec.vcdu_count = 0; // init

    // Case 2: Test pattern mode, specific pattern
    populateVCDU(vcdu, sizeof(vcdu), 0x800F); // Test pattern example (0x800F)
    int32_t parityErrors = assemble_image(vcdu, &imageRec, 0, true, xpos, ypos, &status);

    REQUIRE(parityErrors == 0);
    REQUIRE(status == NOERROR);
    REQUIRE(imageRec.image[0][0] == 0x000F); // Verify top-left pixel is the pattern
}

#ifndef SKIPPARITY
TEST_CASE("Parity Error Handling", "[assemble_image]") {
    uint8_t vcdu[1781] = {0};
    MEGS_IMAGE_REC imageRec;
    std::memset(imageRec.image, 0, sizeof(imageRec.image));
    int32_t xpos, ypos;
    int8_t status = NOERROR;
    imageRec.vcdu_count = 0; // init

    // Case 3: Induce a parity error
    populateVCDU(vcdu, sizeof(vcdu), 0x8001); // odd parity mismatch
    int32_t parityErrors = assemble_image(vcdu, &imageRec, 1, false, xpos, ypos, &status);

    REQUIRE(parityErrors > 0);                  // Expect parity errors to be non-zero
    REQUIRE(status == W_INVALID_PARITY);        // Status should indicate invalid parity
}
#endif

TEST_CASE("Source Sequence Counter Impact", "[assemble_image]") {
    uint8_t vcdu[1781] = {0};
    MEGS_IMAGE_REC imageRec = {0};
    std::memset(imageRec.image, 0, sizeof(imageRec.image));
    int32_t xpos, ypos;
    int8_t status = NOERROR;
    uint16_t ssc=0;
    imageRec.vcdu_count = 0; // init

    // Case 4: Different source sequence counters
    populateVCDU(vcdu, sizeof(vcdu), 0x1FFF); // 2s complement of 0x3fff
    assemble_image(vcdu, &imageRec, ssc, false, xpos, ypos, &status); // Source sequence counter 0
    REQUIRE(imageRec.vcdu_count == 1);
    REQUIRE(imageRec.image[0][0] == 0x3FFF); // this pixel should be set only in ssc=0

    //ssc=1;
    //assemble_image(vcdu, &imageRec, ssc, false, xpos, ypos, &status); // Source sequence counter 1
    //REQUIRE(imageRec.vcdu_count == 2);
    //REQUIRE(imageRec.image[0][0] == 0x3FFF); need to calculate pixel location based on ssc
}


// TEST_CASE("Histogram Equalization on a simple known input image", "[histogramEqualization]") {
//     uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {};

//     // Initialize a known pattern for testing
//     for (int y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
//         for (int x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
//             image[y][x] = (y + x) % 16384; // Simple pattern based on sum of indices
//         }
//     }

//     std::vector<uint8_t> textureData(MEGS_TOTAL_PIXELS);

//     // Call the function under test
//     histogramEqualization(&image, textureData);

//     // Check if the textureData is populated correctly (you can compare with expected values or apply custom logic)
//     // For simplicity, assuming expected values based on your known pattern and histogram equalization process.

//     // Example checks based on the assumed behavior after equalization:
//     REQUIRE(textureData[0] == 0);    // Example check for first pixel
//     REQUIRE(textureData[1] == 0);    // Example check for next pixel
//     REQUIRE(textureData[10] == 5);   // Check a mid-range pixel value
//     // Further checks for other positions depending on your expectations
// }


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
    (packetLength == STANDARD_SHK_PACKET_LENGTH) || \
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

    //std::cout << "Year: " << currentTime.getYear() << "\n";
    //std::cout << "Day of Year: " << currentTime.getDayOfYear() << "\n";
    //std::cout << "Month: " << currentTime.getMonth() << "\n";
    //std::cout << "Day of Month: " << currentTime.getDayOfMonth() << "\n";
    //std::cout << "Hour: " << currentTime.getHour() << "\n";
    std::cout << "Minute: " << currentTime.getMinute() << "\n";
    //std::cout << "Second: " << currentTime.getSecond() << "\n";
    std::cout << "Microseconds since Epoch: " << currentTime.getMicrosecondsSinceEpoch() << "\n";
    std::cout << "UTC Subseconds: " << currentTime.getUTCSubseconds() << "\n";
    std::cout << "TAI Seconds: " << currentTime.getTAISeconds() << "\n";
    std::cout << "TAI Subseconds: " << currentTime.getTAISubseconds() << "\n";

}

// Test suite for the TimeInfo class
TEST_CASE("TimeInfo class functionality", "[TimeInfo]") {
    TimeInfo timeInfo;

    // SECTION("UpdateNow should update time components") {
    //     timeInfo.updateNow();
    //     REQUIRE(timeInfo.getYear() == getCurrentYear());
    // }

    // SECTION("Check Year") {
    //     REQUIRE(timeInfo.getYear() == getCurrentYear());
    // }

    // SECTION("Check Day of Year is within valid range") {
    //     REQUIRE(timeInfo.getDayOfYear() >= 1);
    //     REQUIRE(timeInfo.getDayOfYear() <= 366);  // Account for leap years
    // }

    // SECTION("Check Month is within valid range") {
    //     REQUIRE(timeInfo.getMonth() >= 1);
    //     REQUIRE(timeInfo.getMonth() <= 12);
    // }

    // SECTION("Check Day of Month is within valid range") {
    //     REQUIRE(timeInfo.getDayOfMonth() >= 1);
    //     REQUIRE(timeInfo.getDayOfMonth() <= 31);
    // }

    // SECTION("Check Hour is within valid range") {
    //     REQUIRE(timeInfo.getHour() >= 0);
    //     REQUIRE(timeInfo.getHour() <= 23);
    // }

    SECTION("Check Minute is within valid range") {
        REQUIRE(timeInfo.getMinute() >= 0);
        REQUIRE(timeInfo.getMinute() <= 59);
    }

    // SECTION("Check Second is within valid range") {
    //     REQUIRE(timeInfo.getSecond() >= 0);
    //     REQUIRE(timeInfo.getSecond() <= 60);  // Account for leap seconds
    // }

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