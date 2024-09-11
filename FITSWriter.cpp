#include "FITSWriter.hpp"
#include "commonFunctions.hpp"

// Constructor
FITSWriter::FITSWriter() {}

// Destructor
FITSWriter::~FITSWriter() {
    closeAllFITSFiles();
}

// Function to create a FITS filename based on APID and timestamp
std::string FITSWriter::createFITSFilename(uint16_t apid, double tai_seconds) {
    std::ostringstream oss;
    //oss << "APID_" << apid << "_";

    // Convert TAI seconds into a std::time_t object so it can use built-in conversions for human-readable format (e.g., YYYYMMDD_HHMMSS)
    // std::time_t argument is is seconds since Unix time epoch
    // unixtime is negative before 1970
    int64_t unixtime = tai_seconds - TAI_EPOCH_OFFSET_TO_UNIX;
    if (unixtime > TAI_LEAP_SECONDS) {
        // only apply leap seconds to reasonable times
        unixtime += TAI_LEAP_SECONDS;
    }
    std::time_t t = static_cast<std::time_t>(unixtime);
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

    // This part overwrites an existing FITS file.
    // Check if the file already exists
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) == 0) {
        // File exists, so delete it
        if (std::remove(filename.c_str()) != 0) {
            std::cerr << "ERROR: Failed to delete existing FITS file: " << filename << std::endl;
            return false;
        }
    }

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

    std::cout << "writeMegsAFITS: tai_time_seconds = " << megsStructure.tai_time_seconds << std::endl;

    std::cout<< "writing MEGS-A FITS file" <<std::endl;
    LogFileWriter::getInstance().logInfo("writing MEGSA FITS file");

    std::cout << "writeMegsAFITS 1 vcdu_count " << megsStructure.vcdu_count << " im00"<<" "<<megsStructure.image[0][0] <<" "<< megsStructure.image[1][0] << " "<<megsStructure.image[2][0] <<" "<<megsStructure.image[3][0] <<"\n";

    int32_t status = 0;

    // Create a filename based on APID (assuming 601 for MEGS-A) and the timestamp
    std::string filename = createFITSFilename(601, megsStructure.tai_time_seconds);
    std::cout << "writeMegsAFITS 2 vcdu_count " << megsStructure.vcdu_count << " im00"<<" "<<megsStructure.image[0][0] <<" "<< megsStructure.image[1][0] << " "<<megsStructure.image[2][0] <<" "<<megsStructure.image[3][0] <<"\n";

    // Get the FITS file pointer
    fitsfile* fptr = nullptr;

    // Initialize the FITS file
    if (!initializeFITSImageFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FitsWriter::writeMegsAFITS: Failed to initialize FITS file: " + filename);
        return false;
    }

    // Define the dimensions
    //const uint32_t width = MEGS_IMAGE_WIDTH;
    //const uint32_t height = MEGS_IMAGE_HEIGHT;

    // Create a transposed buffer
    //std::vector<uint16_t> transposedData(width * height);

    //for (uint32_t y = 0; y < height; ++y) {
    //    for (uint32_t x = 0; x < width; ++x) {
    //        transposedData.at(x * height + y) = megsStructure.image[y][x];
    //    }
    //}

    LONGLONG fpixel = 1;

    // This method is transposed, need to transpose ths image
    // const uint16_t (*image)[MEGS_IMAGE_HEIGHT] = megsStructure.image; // no transpose
    // if (fits_write_img(fptr, TUSHORT, fpixel, MEGS_IMAGE_WIDTH * MEGS_IMAGE_HEIGHT, (void*)image, &status)) {
    //     // Log the error
    //     fits_report_error(stderr, status);
    //     LogFileWriter::getInstance().logError("Failed to write image data to FITS file: " + filename);
    //     fits_close_file(fptr, &status); // Ensure the file is closed even if writing fails
    //     return false;
    // }

    // Define the dimensions
    const uint32_t width = MEGS_IMAGE_WIDTH;
    const uint32_t height = MEGS_IMAGE_HEIGHT;

    std::cout<<"writeMegsAFITS orig image"<<std::endl;
    printUint16ToStdOut(megsStructure.image[0], MEGS_IMAGE_WIDTH, 10);
    std::cout << "writeMegsAFITS 3 vcdu_count " << megsStructure.vcdu_count << " im00"<<" "<<megsStructure.image[0][0] <<" "<< megsStructure.image[1][0] << " "<<megsStructure.image[2][0] <<" "<<megsStructure.image[3][0] <<"\n";

    // Create a 1d buffer to hold the transposed data
    std::vector<uint16_t> transposedData(width * height);

    // Transpose the data using parallel processing, enable this pragma
    #pragma omp parallel for
    // Transpose the data
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // this way is flipped vertically
            transposedData[x + (y * width)] = megsStructure.image[x][y];
        }
    }

	//  Fill a header with important information
	fits_write_key_str(fptr, "EXTNAME", "MEGS_IMAGE", "Extension Name", &status);
    if (status != 0) {
        fits_report_error(stderr, status);
        return false;
    }
	fits_write_key(fptr, TUINT, "sod", (void *)&megsStructure.sod, "Seconds in day", &status);
    if (status != 0) {
        fits_report_error(stderr, status);
        return false;
    }
	fits_write_key(fptr, TUINT, "doy", (void *)&megsStructure.yyyydoy, "Year - Day of year", &status);
    if (status != 0) {
        fits_report_error(stderr, status);
        return false;
    }
	fits_write_key(fptr, TUINT, "tai_time", (void *)&megsStructure.tai_time_seconds, "tai time", &status);
    if (status != 0) {
        fits_report_error(stderr, status);
        return false;
    }
    //TODO: deal with filename
	//char tlm_filename[128];
	//char *lastslash = strrchr(filenames[0], '/'); // Search for the LAST '/' in the path	
	//lastslash ++; // Increment the pointer to '/' by one which is the first char in the file name
	//strncpy(tlm_filename, lastslash, 47); //  44 is tlm file name length including the . but no extension
    //	tlm_filename[47] = '\0';

	//fits_write_history(fptr, tlm_filename, &status);
    //if (status != 0) {
    //    fits_report_error(stderr, status);
    //    return false;
    //}


     // Write the transposed data to the FITS file
    status = 0;
    if (fits_write_img(fptr, TUSHORT, fpixel, width * height, transposedData.data(), &status)) {
        // Log the error
        LogFileWriter::getInstance().logError("Failed to write image data to FITS file: " + filename);
        fits_close_file(fptr, &status); // Ensure the file is closed even if writing fails
        return false;
    }
   
    // add code here for a binary table
    //status = 0; // You must ALWAYS set the status value to zero before calling ANY fits routine

    //fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames, fields, unitnames, extname, &status);


    // Close ths FITS file
    if (fits_close_file(fptr, &status)){
        fits_report_error(stderr, status);
        return false;
    }

    std::cout << "FITSWriter::MegsAFITS successfully wrote "<<filename << std::endl;
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
