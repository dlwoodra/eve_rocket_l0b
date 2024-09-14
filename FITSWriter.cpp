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

// Error handling function
void checkFitsStatus(int status) {
    if (status) {
        fits_report_error(stderr, status);
        exit(status);
    }
}

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

// Helper function to convert vector of strings to array of char pointers
std::vector<char*> convertToCharPtrArray(const std::vector<std::string>& vec) {
    std::vector<char*> charArray(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        charArray[i] = const_cast<char*>(vec[i].c_str());
    }
    return charArray;
}

// Helper function to convert a single string to array of char pointers
std::vector<char*> convertTypesToCharPtrArray(const std::string& types) {
    std::vector<char*> typeArray(types.size());
    for (size_t i = 0; i < types.size(); ++i) {
        typeArray[i] = new char[2];  // Allocate space for single char + null terminator
        typeArray[i][0] = types[i];
        typeArray[i][1] = '\0';  // Null terminator
    }
    return typeArray;
}

std::vector<char*> convertTypesToTFormPtrArray(const std::string& types) {
    std::vector<char*> tFormArray(types.size());
    for (size_t i = 0; i < types.size(); ++i) {
        tFormArray[i] = new char[4];  // Allocate space for 3 chars + null terminator
        if (types[i] != 'A') {
            tFormArray[i][0] = '1';
            tFormArray[i][1] = types[i];
            tFormArray[i][2] = '\0';
        } else {
            tFormArray[i][0] = '1';
            tFormArray[i][0] = '6';
            tFormArray[i][2] = 'A';
            tFormArray[i][3] = '\0';
        }
    }
    return tFormArray;
}

// Convert std::vector<std::string> to char**
char** vectorToCharArray(const std::vector<std::string>& vec) {
    char** arr = new char*[vec.size()];
    for (size_t i = 0; i < vec.size(); ++i) {
        arr[i] = new char[vec[i].size() + 1];
        std::strcpy(arr[i], vec[i].c_str());
    }
    return arr;
}

// Free memory allocated for char**
void freeCharArray(char** arr, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        delete[] arr[i];
    }
    delete[] arr;
}

// Function to write a binary table to a FITS file
int writeBinaryTable(const std::string& filename, const void* data, int columns,
                     const std::vector<std::string>& names, 
                    std::string& types,
                     const std::vector<std::string>& units, 
                     bool useUnits, const std::string& extname) {
    int status = 0;  // CFITSIO status value
    fitsfile* fptr = nullptr;  // Pointer to the FITS file

    std::cout << "writeBinaryTable data 1"<<std::endl;
    //printBytes(data, 26 ); //shows 26 bytes - same as writeMegsAFITS

    // Lock file mechanism
    std::string lockfile = filename + ".lock";
    if (access(lockfile.c_str(), F_OK) == 0) {
        sleep(0.01);
        if (access(lockfile.c_str(), F_OK) == 0) {
            return -1;  // File is locked
        }
    } else {
        FILE* flock = fopen(lockfile.c_str(), "w");
        if (flock) {
            fprintf(flock, "File locked by L0B\n");
            fclose(flock);
        }
    }

    // Convert column names and units to char* arrays
    std::vector<char*> colnames = convertToCharPtrArray(names);
    std::vector<char*> unitnames;
    if (useUnits) {
        unitnames = convertToCharPtrArray(units);
    }

    // Convert types to char* array
    std::vector<char*> typeArray = convertTypesToCharPtrArray(types); // "U,V, etc"
    std::vector<char*> tFormArray = convertTypesToTFormPtrArray(types); // "1U,1V, etc"

    std::cout<<"writeBinaryTable sizeof(tFormArray) " <<sizeof(tFormArray.data())<< std::endl;
    printBytes(tFormArray.data(), sizeof(tFormArray.data()));

    // Open or create FITS file
    if (fits_open_file(&fptr, filename.c_str(), READWRITE, &status)) {
        if (fits_create_file(&fptr, filename.c_str(), &status)) {
            checkFitsStatus(status);
        } else {
            // Create binary table HDU if file already exists
            if (useUnits) {
                // args         file, BINTABL,nrows, tfields,           ttype,            tform,            tunit,         extname, &status
                //fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames.data(), typeArray.data(), unitnames.data(), extname.c_str(), &status);
                fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames.data(), tFormArray.data(), unitnames.data(), extname.c_str(), &status);
            } else {
                fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames.data(), tFormArray.data(), nullptr, extname.c_str(), &status);
            }
            checkFitsStatus(status);
        }
    } else {
        // Move to the existing binary table HDU
        char* extname_cstr = const_cast<char*>(extname.c_str());
        fits_movnam_hdu(fptr, BINARY_TBL, extname_cstr, 0, &status);
        if (status) {
            status = 0;
            if (useUnits) {
                fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames.data(), tFormArray.data(), unitnames.data(), extname.c_str(), &status);
            } else {
                fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames.data(), tFormArray.data(), nullptr, extname.c_str(), &status);
            }
            checkFitsStatus(status);
        }
    }

    // Write data to the table
    long firstrow = 1;
    long firstelem = 1;
    const char* pdata = static_cast<const char*>(data);

    std::vector<int> typeCode = {TBYTE, TSBYTE, TSHORT, TUSHORT, TUINT, TULONG, TINT, TUINT, TBIT, TFLOAT, TDOUBLE};

    for (int i = 0; i < columns; ++i) {

        int colType = 0;
        int colTypeSize = 1; //bytes
        if (types[i] == 'B') {
            colType = TBYTE;        // unsigned byte
            colTypeSize = 1;
        } else if (types[i] == 'S') {
            colType = TSBYTE;  // signed byte
            colTypeSize = 1;
        } else if (types[i] == 'X') {
            colType = TBIT;
            colTypeSize = 1;
        } else if (types[i] == 'L') {
            colType = TLOGICAL;
            colTypeSize = 1;
        } else if (types[i] == 'A') {
            colType = TSTRING; // chars
            colTypeSize = 16;
        } else if (types[i] == 'I') {
            colType = TSHORT;  // int16
            colTypeSize = 2;
        } else if (types[i] == 'J') {
            colType = TINT;    // int32
            colTypeSize = 4;
        } else if (types[i] == 'K') {
            colType = TLONG;   // int64
            colTypeSize = 8;
        } else if (types[i] == 'E') {
            colType = TFLOAT;  // 32-bit float
            colTypeSize = 4;
        } else if (types[i] == 'D') {
            colType = TDOUBLE; // 64-bit complex
            colTypeSize = 8;
        } else if (types[i] == 'U') {
            colType = TUSHORT; // uint16
            colTypeSize = 2;
        } else if (types[i] == 'V') {
            colType = TUINT;  // uint32 (TULONG if 64-bit)
            //types[i] = 'U';
            colTypeSize = 4;
        } else {
            std::cerr << "Unknown type code: " << types[i] << std::endl;
            return -1;
        }

        std::cout << "coltype: " << colType << " " << std::hex << *pdata << std::dec<<std::endl;

        //int colType = typeCode[i];  // Map typeCode to CFITSIO type code
        fits_write_col(fptr, colType, i + 1, firstrow, firstelem, 1, (void*)pdata, &status);
        checkFitsStatus(status);

        //pdata += sizeof(uint8_t);  // Adjust based on the type of data
        pdata += colTypeSize;
    }

    // Close the FITS file
    fits_close_file(fptr, &status);
    checkFitsStatus(status);

    // Remove lock file
    remove(lockfile.c_str());

    // Free allocated memory
    //freeCharArray(colnames, colnames.size());
    //freeCharArray(typeArray, typeArray.size());

    return 0;
}

// write the megs rec to the FITS file
bool FITSWriter::writeMegsAFITS( const MEGS_IMAGE_REC& megsStructure) {

    //std::cout << "writeMegsAFITS: tai_time_seconds = " << megsStructure.tai_time_seconds << std::endl;

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
    checkFitsStatus(status);

	fits_write_key(fptr, TUINT, "sod", (void *)&megsStructure.sod, "UTC Seconds of day of first packet", &status);
    checkFitsStatus(status);

	fits_write_key(fptr, TUINT, "ydoy", (void *)&megsStructure.yyyydoy, "7-digit UTC Year and Day of year", &status);
    checkFitsStatus(status);

	fits_write_key(fptr, TUINT, "tai_time", (void *)&megsStructure.tai_time_seconds, "tai seconds from first packet", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TUINT, "rec_tai", (void *)&megsStructure.rec_tai_seconds , "computer receive time in tai seconds", &status);
    checkFitsStatus(status);

    //std::cout<<"writeMegsAFITS iso8601 "<<megsStructure.iso8601<<std::endl;
	fits_write_key(fptr, TSTRING, "DATE-BEG", const_cast<char*>((megsStructure.iso8601).c_str()), "UTC from first packet", &status);
    checkFitsStatus(status);

    float solarnet=0.5;
	fits_write_key(fptr, TFLOAT, "SOLARNET", (void *)&solarnet, "Partially SOLARNET compliant", &status);
    checkFitsStatus(status);

    int obs_hdu=1;
	fits_write_key(fptr, TUINT, "OBS_HDU", (void *)&obs_hdu, "Partially SOLARNET compliant", &status);
    checkFitsStatus(status);

     // Write the transposed data to the FITS file
    status = 0;
    if (fits_write_img(fptr, TUSHORT, fpixel, width * height, transposedData.data(), &status)) {
        // Log the error
        LogFileWriter::getInstance().logError("Failed to write image data to FITS file: " + filename);
        fits_close_file(fptr, &status); // Ensure the file is closed even if writing fails
        return false;
    }
   
    status = 0; // You must ALWAYS set the status value to zero before calling ANY fits routine

    // Close ths FITS file
    fits_close_file(fptr, &status);
    checkFitsStatus(status);

    // add code here for a binary table

    // old way
    //fits_create_tbl(fptr, BINARY_TBL, 0, columns, colnames, fields, unitnames, extname, &status);

    // Define column names
    std::vector<std::string> columnNames = {
        "YYYYDOY", "SOD", "TAI_TIME_SECONDS", "TAI_TIME_SUBSECONDS",
        "REC_TAI_SECONDS", "REC_TAI_SUBSECONDS", "VCDU_COUNT"
    };

    // Define column types (corresponding to FITS types)
    std::vector<std::string> columnTypes = {
        "V", "V", "V", "V", 
        "V", "V", "U"
    };
    // Convert column types to a single joined string
    std::string typesString = "";
    for (const auto& type : columnTypes) {
        typesString += type;
    }

    // Units in this case
    std::vector<std::string> columnUnits {
        "DATE", "s", "s", "s", 
        "s", "s", "count"
    };

    // Define extension name
    std::string extname = "MEGSA_TABLE";

    //copy data
    struct DataRow {
        uint32_t yyyydoy;
        uint32_t sod;
        uint32_t tai_time_seconds;
        uint32_t tai_time_subseconds;
        uint32_t rec_tai_seconds;
        uint32_t rec_tai_subseconds;
        uint16_t vcdu_count;
    } __attribute__((packed));

    // Allocate memory for one row of data
    std::vector<uint8_t> data(sizeof(DataRow));
    DataRow row = { megsStructure.yyyydoy, 
        megsStructure.sod, 
        megsStructure.tai_time_seconds, 
        megsStructure.tai_time_subseconds,
        megsStructure.rec_tai_seconds, 
        megsStructure.rec_tai_subseconds, 
        megsStructure.vcdu_count
        };
    // Copy the data into the vector
    memcpy(data.data(), &row, sizeof(DataRow)); // without packing sizeof(DataRow) is 2 bytes larger due to padding
    std::cout<<"writeMegsAFITS 4: sizeof(DataRow) "<< sizeof(DataRow)<<" sizeof(data.data()) "<< sizeof(data.data()) <<std::endl;    
    printBytesToStdOut(data.data(),0,sizeof(DataRow)-1);
    printBytes(&row, sizeof(row)); //shows 26 bytes

    int result = writeBinaryTable(
        filename,           // FITS file name
        data.data(),        // Pointer to loaded data
        columnNames.size(), // Number of columns (7)
        columnNames,        // Column names
        typesString,        // Column FITS types
        columnUnits,        // Column Units
        true,               // Use Units
        extname.c_str()     // Extension name
        );
    std::cout << "writeBinaryTable result: " << result << std::endl;
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
