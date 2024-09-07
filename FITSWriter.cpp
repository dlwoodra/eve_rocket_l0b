#include "FITSWriter.hpp"
#include "commonFunctions.hpp"

// Constructor
FITSWriter::FITSWriter() {}

// Destructor
FITSWriter::~FITSWriter() {
    closeAllFITSFiles();
}

// Function to create a FITS filename based on APID and timestamp
std::string FITSWriter::createFITSFilename(uint16_t apid, double timestamp) {
    std::ostringstream oss;
    //oss << "APID_" << apid << "_";
    
    // Convert timestamp to human-readable format (e.g., YYYYMMDD_HHMMSS)
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::gmtime(&t);

    std::string filename_prefix;
    if ( apid == 601 ) {
        filename_prefix = "MA__L0B_0_";
    } else if ( apid == 602 ) {
        filename_prefix = "MB__L0B_0_";
    } else if ( apid == 604 ) {
        filename_prefix = "MP__L0B_0_";
    } else if ( apid == 605 ) {
        filename_prefix == "ESP_L0B_0_";
    } else {filename_prefix = "unknown_apid_";}

    oss << filename_prefix << std::put_time(tm, "%Y%j_%H%M%S") << ".fit";
    
    return oss.str();
}

// Function to initialize a FITS file
bool FITSWriter::initializeFITSImageFile(const std::string& filename, fitsfile*& fptr) {
    //fitsfile* fptr;
    int status = 0;

    // Create a new FITS file
    if (fits_create_file(&fptr, filename.c_str(), &status)) {
        fits_report_error(stderr, status);
        return false;
    }

    // Create a primary array image (e.g., 16-bit unsigned integer)
    long naxes[2] = {MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT}; // dimensions, can be adjusted as needed
    if (fits_create_img(fptr, USHORT_IMG, 2, naxes, &status)) {
        fits_report_error(stderr, status);
        return false;
    }

    return true;
}

// Function to write packet data to the appropriate FITS file based on APID and timestamp
//bool FITSWriter::writePacketToFITS(const std::vector<uint8_t>& packet, uint16_t apid, double timestamp) {
 //   std::string filename = createFITSFilename(apid, timestamp);

    // this line may not be needed
//    std::unordered_map<std::string, std::unique_ptr<fitsfile>> fitsFileMap;

    // Check if the FITS file already exists in the map
    //if (fitsFileMap.find(filename) == fitsFileMap.end()) {
    //if (fitsFileMap.find(filename) != fitsFileMap.end()) {
    //    // If not, initialize the FITS file
    //    if (!initializeFITSFile(filename)) {
    //        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
     //       return false;
    //    }
    //}

    // Write the data to the FITS file
//    fitsfile* fptr = fitsFileMap[filename].get();
//    return writeDataToFITS(fptr, packet);
//}

// Function to write data to a FITS file
bool FITSWriter::writeDataToFITS(fitsfile* fptr, const std::vector<uint8_t>& data) {
    int status = 0;
    //long fpixel[2] = {1, 1}; // Starting point to write in the image
    long long int fpixel = 1; // Starting point to write in the image

    // Write data to the image
    if (fits_write_img(fptr, TBYTE, fpixel, data.size(), const_cast<uint8_t*>(data.data()), &status)) {
        fits_report_error(stderr, status);
        return false;
    }

    return true;
}

// write the megs rec to the FITS file
bool FITSWriter::writeMegsAFITS( const MEGS_IMAGE_REC& megsStructure) {

    std::cout<< "writing MEGS-A FITS file" <<std::endl;
    LogFileWriter::getInstance().logInfo("writing MEGSA FITS file");

    int32_t status = 0;

    // Create a filename based on APID (assuming 601 for MEGS-A) and the timestamp
    std::string filename = createFITSFilename(601, megsStructure.tai_time_seconds);

    // Get the FITS file pointer
    fitsfile* fptr = nullptr;

    // Initialize the FITS file
    if (!initializeFITSImageFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FitsWriter::writeMegsAFITS: Failed to initialize FITS file: " + filename);
        return false;
    }

    LONGLONG fpixel = 1;

    // Write the MEGS image to the FITS file
    uint16_t* tempBuffer = new uint16_t[MEGS_IMAGE_WIDTH * MEGS_IMAGE_HEIGHT];
    std::memcpy(tempBuffer, megsStructure.image, sizeof(uint16_t) * MEGS_IMAGE_WIDTH * MEGS_IMAGE_HEIGHT);
    if (fits_write_img(fptr, TUSHORT, fpixel, MEGS_IMAGE_WIDTH * MEGS_IMAGE_HEIGHT, (void*)tempBuffer, &status)) {
        fits_report_error(stderr, status);
        std::ostringstream oss;
        oss << "FitsWriter::wrieMegsAFITS: Failed to write image data to FITS file: " << filename;
        LogFileWriter::getInstance().logError(oss.str());
        delete[] tempBuffer;
        return false;
    }
    delete[] tempBuffer;

    // add code here for a binary table

    // Close ths FITS file
    if (fits_close_file(fptr, &status)){
        fits_report_error(stderr, status);
        return false;
    }

    std::cout << "FITSWriter::MegsAFITS successfully wrote"<<filename << std::endl;
    LogFileWriter::getInstance().logInfo("FITSWriter::MegsAFITS successfully wrote " + filename);

    return true;
}

// Function to close all open FITS files
void FITSWriter::closeAllFITSFiles() {
    for (auto& kv : fitsFileMap) {
        if (kv.second) {
            int status = 0;
            fits_close_file(kv.second.get(), &status);
            if (status) {
                fits_report_error(stderr, status);
            }
        }
    }
    fitsFileMap.clear();
}


//#include <fitsio.h>
//#include <iostream>
//#include <vector>
//#include <cstdint>
//#include <string>
//#include <chrono>
//#include <iomanip>
//#include <sstream>

//class FITSWriter {
//public:
//    FITSWriter();
//    ~FITSWriter();

//    bool writePacketToFITS(const std::vector<uint8_t>& packet, uint16_t apid, double timeStamp);

//private:
//    int status;

//    std::string createFilename(uint16_t apid, double timeStamp);
 //   void createAndWriteFITSFile(const std::string& filename, const std::vector<uint8_t>& packet, double timeStamp);
//};

//FITSWriter::FITSWriter() : status(0) {}

//FITSWriter::~FITSWriter() {}

//std::string FITSWriter::createFilename(uint16_t apid, double timeStamp) {
 //   // Get current time
 //   auto now = std::chrono::system_clock::now();
 //   auto time_t_now = std::chrono::system_clock::to_time_t(now);
 //   auto tm_now = *std::localtime(&time_t_now);

    // Create a stringstream to format the filename
 //   std::stringstream ss;
 //   ss << "APID_" << apid << "_";
 //   ss << std::put_time(&tm_now, "%Y_%j_%H_%M_%S"); // Year_DayOfYear_Hour_Minute_Second
 //   ss << "_" << static_cast<int>(timeStamp) << ".fits";

//    return ss.str();
//}

//void FITSWriter::createAndWriteFITSFile(const std::string& filename, const std::vector<uint8_t>& packet, double timeStamp) {
//    fitsfile* fptr;
//    fits_create_file(&fptr, filename.c_str(), &status);
//    if (status) {
//        fits_report_error(stderr, status);
//        exit(EXIT_FAILURE);
//    }

//    long naxes[2] = { packet.size(), 1 };  // Example for a 1D array
//    fits_create_img(fptr, BYTE_IMG, 1, naxes, &status);
//    if (status) {
//        fits_report_error(stderr, status);
//        exit(EXIT_FAILURE);
//    }

 //   long fpixel = 1; // starting pixel
 //   fits_write_img(fptr, TBYTE, fpixel, packet.size(), const_cast<uint8_t*>(packet.data()), &status);
 //   if (status) {
 //       fits_report_error(stderr, status);
 //       exit(EXIT_FAILURE);
 //   }

    // Add timestamp or other metadata to the header if needed
//    fits_update_key(fptr, TDOUBLE, "TIMESTAMP", &timeStamp, "Packet timestamp", &status);
//    if (status) {
//        fits_report_error(stderr, status);
//        exit(EXIT_FAILURE);
//    }

//    fits_close_file(fptr, &status);
//    if (status) {
//        fits_report_error(stderr, status);
//        exit(EXIT_FAILURE);
//    }
//}

//bool FITSWriter::writePacketToFITS(const std::vector<uint8_t>& packet, uint16_t apid, double timeStamp) {
//    std::string filename = createFilename(apid, timeStamp);
//    createAndWriteFITSFile(filename, packet, timeStamp);
//    return true;
//}
