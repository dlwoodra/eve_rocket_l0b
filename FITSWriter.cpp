#include "FITSWriter.hpp"
#include "commonFunctions.hpp"

extern std::vector<uint16_t> transposeImageTo1D(const uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT]);

// Constructor
FITSWriter::FITSWriter() {}

// Destructor
FITSWriter::~FITSWriter() {}

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

    static const char* env_eve_data_root = std::getenv("eve_data_root");
    if (env_eve_data_root == nullptr) {
        std::cout<< "***";
        std::cout << "ERROR: environment variable eve_data_root is undefined - aborting" <<std::endl;
        std::cout<< "***";
        exit(EXIT_FAILURE);
    }
    static const std::string dataRootPath(env_eve_data_root);
    std::string l0bpath = dataRootPath + "level0b/";

    std::string filename_prefix;
    std::string channelstring;

    switch (apid) {
        case MEGSA_APID:
            channelstring = "megs_a/";
            filename_prefix =  "MA__L0B_0_";
            break;
        case MEGSB_APID:
            channelstring = "megs_b/";
            filename_prefix = "MB__L0B_0_";
            break;
        case MEGSP_APID:
            channelstring = "megs_p/";
            filename_prefix = "MP__L0B_0_";
            break;
        case ESP_APID:
            channelstring = "esp/";
            filename_prefix = "ESP_L0B_0_";
            break;
        case HK_APID:
            channelstring = "shk/";
            filename_prefix = "SHK_L0B_0_";
            break;
        default:
            channelstring = "unk/";
            filename_prefix = "unknown_apid_" + std::to_string(apid) + "_";
            break;
    }

    oss << l0bpath << channelstring << std::put_time(tm, "%Y/%j/");

    // Create the directories if they don't exist, use system call
    std::string dirPath = oss.str();
    if (!create_directory_if_not_exists(dirPath)) {
        return "";
    }

    oss << filename_prefix << std::put_time(tm, "%Y%j_%H%M%S") << ".fit";

    std::cout<< "createFITSFilename - " << oss.str() << std::endl;

    return oss.str();
}

// Function to initialize a FITS file
bool FITSWriter::initializeFITSFile(const std::string& filename, fitsfile*& fptr) {
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

    return true;
}

// Error handling function
void FITSWriter::checkFitsStatus(int status) {
    if (status) {
        fits_report_error(stderr, status);
        exit(status);
    }
}

// Helper function to convert vector of strings to array of char pointers
std::vector<char*> FITSWriter::convertToCharPtrArray(const std::vector<std::string>& vec) {
    std::vector<char*> charArray(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        charArray[i] = const_cast<char*>(vec[i].c_str());
    }
    return charArray;
}

// Helper function to convert a single string to array of char pointers
std::vector<char*> FITSWriter::convertTypesToCharPtrArray(const std::string& types) {
    std::vector<char*> typeArray(types.size());
    for (size_t i = 0; i < types.size(); ++i) {
        typeArray[i] = new char[2];  // Allocate space for single char + null terminator
        typeArray[i][0] = types[i];
        typeArray[i][1] = '\0';  // Null terminator
    }
    return typeArray;
}

std::vector<char*> FITSWriter::convertTypesToTFormPtrArray(const std::string& types, const std::vector<int>& lengths) {
    std::vector<char*> tFormArray(types.size());
    for (size_t i = 0; i < types.size(); ++i) {
        tFormArray[i] = new char[10];  // Allocate space for 9 chars + null terminator
        sprintf(tFormArray[i], "%d%c", lengths[i], types[i]);
    }
    return tFormArray;
}

void FITSWriter::freeCharPtrArray(std::vector<char*>& array) {
    for (size_t i = 0; i < array.size(); ++i) {
        delete[] array[i];  // Free each char array
    }
    //array.clear();  // Optional: Clear the vector after cleanup
}

// Function to write a binary table to a FITS file
int FITSWriter::writeBinaryTable(const std::string& filename, 
        const void* data, 
        int columns,
        const std::vector<std::string>& names, 
        const std::string& types,
        const std::vector<int>& columnLengths,
        const std::vector<std::string>& units, 
        bool useUnits, const std::string& extname) {

    int status = 0;  // CFITSIO status value
    fitsfile* fptr = nullptr;  // Pointer to the FITS file

    std::cout << "writeBinaryTable data 1"<<std::endl;

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
    std::vector<char*> tFormArray = convertTypesToTFormPtrArray(types, columnLengths); // "1U,1V, etc"

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
            colType = TDOUBLE; // 64-bit double
            colTypeSize = 8;
        } else if (types[i] == 'U') {
            colType = TUSHORT; // uint16
            colTypeSize = 2;
        } else if (types[i] == 'V') {
            colType = TUINT;  // uint32 (TULONG if 64-bit)
            colTypeSize = 4;
        } else {
            std::cerr << "Unknown type code: " << types[i] << std::endl;
            return -1;
        }

        fits_write_col(fptr, colType, i + 1, firstrow, firstelem, columnLengths[i], (void*)pdata, &status);
        checkFitsStatus(status);

        pdata += colTypeSize; // Adjusts based on the type of data
    }

    // Close the FITS file
    fits_close_file(fptr, &status);
    checkFitsStatus(status);

    // Remove lock file
    remove(lockfile.c_str());

    // Free allocated memory
    freeCharPtrArray(typeArray);
    freeCharPtrArray(tFormArray);

    return 0;
}

// write FITS image header keywords and values
bool FITSWriter::writeMegsFITSImgHeader(fitsfile* fptr, const MEGS_IMAGE_REC& megsStructure, int32_t& status) {
    fits_write_key_str(fptr, "EXTNAME", "MEGS_IMAGE", "Extension Name", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TUINT, "sod", (void *)&megsStructure.sod, "UTC Seconds of day of first packet", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TUINT, "ydoy", (void *)&megsStructure.yyyydoy, "7-digit UTC Year and Day of year", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TUINT, "tai_time", (void *)&megsStructure.tai_time_seconds, "tai seconds from first packet", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TUINT, "rec_tai", (void *)&megsStructure.rec_tai_seconds, "computer receive time in tai seconds", &status);
    checkFitsStatus(status);

    fits_write_key(fptr, TSTRING, "DATE-BEG", const_cast<char*>((megsStructure.iso8601).c_str()), "UTC from first packet", &status);
    checkFitsStatus(status);

    float solarnet = 0.5;
    fits_write_key(fptr, TFLOAT, "SOLARNET", &solarnet, "Partially SOLARNET compliant", &status);
    checkFitsStatus(status);

    int obs_hdu = 1;
    fits_write_key(fptr, TUINT, "OBS_HDU", &obs_hdu, "Partially SOLARNET compliant", &status);
    checkFitsStatus(status);

    return (status == 0);
}

// write the MEGS binary tables in HDU1
int FITSWriter::writeMegsFITSBinaryTable(
    const std::string& filename,
    const MEGS_IMAGE_REC& megsStructure,
    const std::string& extname,
    uint16_t apid
) {
    std::vector<std::string> columnNames = {
        "YYYYDOY", "SOD", "TAI_TIME_SECONDS", "TAI_TIME_SUBSECONDS",
        "REC_TAI_SECONDS", "REC_TAI_SUBSECONDS", "VCDU_COUNT"
    };

    std::vector<std::string> columnTypes = {"V", "V", "V", "V", "V", "V", "U"};
    std::vector<int> columnLengths = {1,1,1,1,1,1,1};
    std::string combinedColumnTypes;    
    for (const auto& type : columnTypes) {
        combinedColumnTypes += type;  // Concatenate each string in columnTypes
    }
    std::vector<std::string> columnUnits = {"DATE", "s", "s", "s", "s", "s", "count"};

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

    DataRow row = {
        megsStructure.yyyydoy, megsStructure.sod, megsStructure.tai_time_seconds,
        megsStructure.tai_time_subseconds, megsStructure.rec_tai_seconds,
        megsStructure.rec_tai_subseconds, megsStructure.vcdu_count
    };

    std::vector<uint8_t> data(sizeof(DataRow));
    memcpy(data.data(), &row, sizeof(DataRow));

    return writeBinaryTable(
        filename, data.data(), columnNames.size(), columnNames,
        combinedColumnTypes, columnLengths, columnUnits, true, extname.c_str()
    );
}

// primary function to manage FITS file writing for CCD images with a binary table
bool FITSWriter::writeMegsFITS(const MEGS_IMAGE_REC& megsStructure, uint16_t apid, const std::string& extname) {
    std::cout << "writing MEGS FITS file for APID: " << apid << std::endl;
    LogFileWriter::getInstance().logInfo("writing MEGS FITS file");

    int32_t status = 0;
    std::string filename = createFITSFilename(apid, megsStructure.tai_time_seconds);

    fitsfile* fptr = nullptr;
    if (!initializeFITSFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FITSWriter::writeMegsFITS: Failed to initialize FITS file: {}", filename);
        return false;
    }

    // Create a primary array image (e.g., 16-bit unsigned integer)
    long naxes[2] = {MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT}; // dimensions
    if (fits_create_img(fptr, USHORT_IMG, 2, naxes, &status)) {
        fits_report_error(stderr, status);
        return false;
    }


    if (!writeMegsFITSImgHeader(fptr, megsStructure, status)) {
        return false;
    }

    std::vector<uint16_t> transposedData = transposeImageTo1D(megsStructure.image);
    // uint16_t tData[MEGS_IMAGE_T_WIDTH][MEGS_IMAGE_T_HEIGHT];
    // // reverse in x direction
    // #pragma omp parallel for
    // for (int32_t i = 0; i < MEGS_IMAGE_T_WIDTH; ++i) { //1024
    //     for (int32_t j = 0; j < MEGS_IMAGE_T_HEIGHT; ++j) { //2048
    //         tData[i][j] = megsStructure.image[i][MEGS_IMAGE_T_HEIGHT - j]; // reverse data along x axis
    //     }
    // }

    //void* ptrimage = (void*) (tData); //1024x2048

    LONGLONG fpixel = 1;
    if (fits_write_img(fptr, TUSHORT, fpixel, transposedData.size(), transposedData.data(), &status)) {
    //if (fits_write_img(fptr, TUSHORT, fpixel, MEGS_IMAGE_T_HEIGHT * MEGS_IMAGE_T_WIDTH, ptrimage, &status)) {
        LogFileWriter::getInstance().logError("Failed to write image data to FITS file: {}", filename);
        fits_close_file(fptr, &status);
        return false;
    }

    fits_close_file(fptr, &status);
    checkFitsStatus(status);

    int result = writeMegsFITSBinaryTable(filename, megsStructure, extname, apid);
    if (result != 0) {
        std::cerr << "Failed to write binary table to FITS file: " << filename << std::endl;
        return false;
    }

    std::cout << "FITSWriter::writeMegsFITS successfully wrote " << filename << std::endl;
    LogFileWriter::getInstance().logInfo("FITSWriter::writeMegsFITS successfully wrote {}", filename);

    return true;
}

// MEGS-A wrapper function to the common writeMegsFITS
bool FITSWriter::writeMegsAFITS( const MEGS_IMAGE_REC& megsStructure) {
    return writeMegsFITS(megsStructure, MEGSA_APID, "MEGSA_TABLE");
}

// MEGS-B wrapper function to the common writeMegsFITS
bool FITSWriter::writeMegsBFITS( const MEGS_IMAGE_REC& megsStructure) {
    return writeMegsFITS(megsStructure, MEGSB_APID, "MEGSB_TABLE");
}

// MEGS-P binary table writer
int FITSWriter::writeMegsPFITSBinaryTable(const std::string& filename, const MEGSP_PACKET& megsPStructure) {
    
    std::string extname = "MEGSP_DATA";
    std::vector<std::string> columnNames = {
        "YYYYDOY", "SOD", "TAI_TIME_SECONDS", "TAI_TIME_SUBSECONDS",
        "REC_TAI_SECONDS", "REC_TAI_SUBSECONDS", 
        "MP_lya", "MP_dark" 
    };

    std::vector<std::string> columnTypes = {"V", "V", "V", "V", 
        "V", "V", 
        "U", "U"};
    std::string combinedColumnTypes;    
    for (const auto& type : columnTypes) {
        combinedColumnTypes += type;  // Concatenate each string in columnTypes
    }
    std::vector<std::string> columnUnits = {"DATE", "s", "s", "s", "s", "s", "count","count"};

    std::vector<int> columnLengths = {1,1,1,1,1,1,MEGSP_INTEGRATIONS_PER_FILE, MEGSP_INTEGRATIONS_PER_FILE};

    //copy data
    struct DataRow {
        uint32_t yyyydoy;
        uint32_t sod;
        uint32_t tai_time_seconds;
        uint32_t tai_time_subseconds;
        uint32_t rec_tai_seconds;
        uint32_t rec_tai_subseconds;
        uint16_t MP_lya[MEGSP_INTEGRATIONS_PER_FILE];
        uint16_t MP_dark[MEGSP_INTEGRATIONS_PER_FILE];
    } __attribute__((packed));

    // populate the scalars
    DataRow row = {
        megsPStructure.yyyydoy, megsPStructure.sod, megsPStructure.tai_time_seconds,
        megsPStructure.tai_time_subseconds, megsPStructure.rec_tai_seconds,
        megsPStructure.rec_tai_subseconds
    };
    // populate the arrays
    for (size_t i = 0; i < MEGSP_INTEGRATIONS_PER_FILE; ++i) {
        row.MP_lya[i] = megsPStructure.MP_lya[i];
        row.MP_dark[i] = megsPStructure.MP_dark[i];
    }

    std::vector<uint8_t> data(sizeof(DataRow));
    memcpy(data.data(), &row, sizeof(DataRow));

    return writeBinaryTable(
        filename, data.data(), columnNames.size(), columnNames,
        combinedColumnTypes, columnLengths, columnUnits, true, extname.c_str()
    );
}

// MEGS-P main writer
bool FITSWriter::writeMegsPFITS( const MEGSP_PACKET& megsPStructure) {

    std::cout << "writing MEGS-P FITS file for APID: " << MEGSP_APID << std::endl;
    LogFileWriter::getInstance().logInfo("writing MEGS-P FITS file");

    int32_t status = 0;
    std::string filename = createFITSFilename(MEGSP_APID, megsPStructure.tai_time_seconds);

    fitsfile* fptr = nullptr;
    if (!initializeFITSFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FITSWriter::writeMegsPFITS: Failed to initialize FITS file: {}", filename);
        return false;
    }

    checkFitsStatus(status);

    int result = writeMegsPFITSBinaryTable(filename, megsPStructure);
    if (result != 0) {
        std::cerr << "Failed to write binary table to MEGSP FITS file: " << filename << std::endl;
        return false;
    }

    std::cout << "FITSWriter::writeMegsPFITS successfully wrote " << filename << std::endl;
    LogFileWriter::getInstance().logInfo("FITSWriter::writeMegsPFITS successfully wrote {}", filename);

    return true;

}

// ESP binary table writer
int FITSWriter::writeESPFITSBinaryTable(const std::string& filename, const ESP_PACKET& ESPStructure) {
    
    std::string extname = "ESP_DATA";
    std::vector<std::string> columnNames = {
        "YYYYDOY", "SOD", "TAI_TIME_SECONDS", "TAI_TIME_SUBSECONDS",
        "REC_TAI_SECONDS", "REC_TAI_SUBSECONDS", 
        "ESP_xfer_cnt","ESP_q0", "ESP_q1", "ESP_q2", "ESP_q3", "ESP_171", "ESP_257", "ESP_304", "ESP_366", "ESP_dark" 
    };

    std::vector<std::string> columnTypes = {"V", "V", "V", "V", 
        "V", "V", 
        "U", "U", "U", "U", "U", "U", "U", "U", "U", "U"};
    std::string combinedColumnTypes;    
    for (const auto& type : columnTypes) {
        combinedColumnTypes += type;  // Concatenate each string in columnTypes
    }
    std::vector<std::string> columnUnits = {"DATE", "s", "s", "s", "s", "s", "", "count","count","count", "count","count","count", "count","count","count"};

    std::vector<int> columnLengths = {1,1,1,1,1,1,ESP_INTEGRATIONS_PER_FILE, 
        ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE,
        ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE,
        ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE, ESP_INTEGRATIONS_PER_FILE 
    };

    //copy data
    struct DataRow {
        uint32_t yyyydoy;
        uint32_t sod;
        uint32_t tai_time_seconds;
        uint32_t tai_time_subseconds;
        uint32_t rec_tai_seconds;
        uint32_t rec_tai_subseconds;
        uint16_t ESP_xfer_cnt[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_q0[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_q1[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_q2[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_q3[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_171[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_257[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_304[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_366[ESP_INTEGRATIONS_PER_FILE];
        uint16_t ESP_dark[ESP_INTEGRATIONS_PER_FILE];
    } __attribute__((packed));

    // populate the scalars
    DataRow row = {
        ESPStructure.yyyydoy, ESPStructure.sod, ESPStructure.tai_time_seconds,
        ESPStructure.tai_time_subseconds, ESPStructure.rec_tai_seconds,
        ESPStructure.rec_tai_subseconds
    };
    // populate the arrays
    for (size_t i = 0; i < ESP_INTEGRATIONS_PER_FILE; ++i) {
        row.ESP_xfer_cnt[i] = ESPStructure.ESP_xfer_cnt[i];
        row.ESP_q0[i] = ESPStructure.ESP_q0[i];
        row.ESP_q1[i] = ESPStructure.ESP_q1[i];
        row.ESP_q2[i] = ESPStructure.ESP_q2[i];
        row.ESP_q3[i] = ESPStructure.ESP_q3[i];
        row.ESP_171[i] = ESPStructure.ESP_171[i];
        row.ESP_257[i] = ESPStructure.ESP_257[i];
        row.ESP_304[i] = ESPStructure.ESP_304[i];
        row.ESP_366[i] = ESPStructure.ESP_366[i];
        row.ESP_dark[i] = ESPStructure.ESP_dark[i];
    }

    std::vector<uint8_t> data(sizeof(DataRow));
    memcpy(data.data(), &row, sizeof(DataRow));

    return writeBinaryTable(
        filename, data.data(), columnNames.size(), columnNames,
        combinedColumnTypes, columnLengths, columnUnits, true, extname.c_str()
    );
}

// ESP main writer
bool FITSWriter::writeESPFITS( const ESP_PACKET& ESPStructure) {

    std::cout << "writing ESP FITS file for APID: " << ESP_APID << std::endl;
    LogFileWriter::getInstance().logInfo("writing ESP FITS file");

    int32_t status = 0;
    std::string filename = createFITSFilename(ESP_APID, ESPStructure.tai_time_seconds);

    fitsfile* fptr = nullptr;
    if (!initializeFITSFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FITSWriter::writeESPFITS: Failed to initialize FITS file: {}", filename);
        return false;
    }

    checkFitsStatus(status);

    int result = writeESPFITSBinaryTable(filename, ESPStructure);
    if (result != 0) {
        std::cerr << "Failed to write binary table to ESP FITS file: " << filename << std::endl;
        return false;
    }

    std::cout << "FITSWriter::writeESPFITS successfully wrote " << filename << std::endl;
    LogFileWriter::getInstance().logInfo("FITSWriter::writeESPFITS successfully wrote {}", filename);

    return true;

}

// SHK binary table writer
int FITSWriter::writeSHKFITSBinaryTable(const std::string& filename, const SHK_PACKET& SHKStructure ) {
    
    const std::string extname = "SHK_DATA";
    std::vector<std::string> columnNames = {
        "YYYYDOY", "SOD", "TAI_TIME_SECONDS", "TAI_TIME_SUBSECONDS",
        "REC_TAI_SECONDS", "REC_TAI_SUBSECONDS", 
        "FPGA_Board_Temperature", "FPGA_Board_p5_0_Voltage", 
        "FPGA_Board_p3_3_Voltage", "FPGA_Board_p1_2_Voltage",
        "MEGSA_CEB_Temperature", "MEGSA_CPR_Temperature",
        "MEGSA_p24_Voltage", "MEGSA_p15_Voltage", "MEGSA_m15_Voltage",
        "MEGSA_p5_0_Analog_Voltage", "MEGSA_m5_0_Voltage", 
        "MEGSA_p5_0_Digital_Voltage", "MEGSA_p2_5_Voltage",
        "MEGSA_p24_Current", "MEGSA_p15_Current", "MEGSA_m15_Current", //16
        "MEGSA_p5_0_Analog_Current", "MEGsA_m5_0_Current", 
        "MEGSA_p5_0_Digital_Current", "MEGSA_p2_5_Current", 
        "MEGSA_Integration_Register", "MEGSA_Analog_Mux_Register",
        "MEGSA_Digital_Status_Register", "MEGSA_Integration_Timer_Register",
        "MEGSA_Command_Error_Count_Register", "MEGSA_CEB_FPGA_Version_Register",
        "MEGSB_CEB_Temperature", "MEGSB_CPR_Temperature", //28
        "MEGSB_p24_Voltage", "MEGSB_p15_Voltage", "MEGSB_m15_Voltage",
        "MEGSB_p5_0_Analog_Voltage", "MEGSB_m5_0_Voltage", 
        "MEGSB_p5_0_Digital_Voltage", "MEGSB_p2_5_Voltage",
        "MEGSB_p24_Current", "MEGSB_p15_Current", "MEGSB_m15_Current",
        "MEGSB_p5_0_Analog_Current", "MEGSB_m5_0_Current", //40
        "MEGSB_p5_0_Digital_Current", "MEGSB_p2_5_Current", 
        "MEGSB_Integration_Register", "MEGSB_Analog_Mux_Register",
        "MEGSB_Digital_Status_Register", "MEGSB_Integration_Timer_Register",
        "MEGSB_Command_Error_Count_Register", "MEGSB_CEB_FPGA_Version_Register", //48
        "MEGSA_Thermistor_Diode", "MEGSA_PRT",
        "MEGSB_Thermistor_Diode", "MEGSB_PRT" //52 + 6 from common time stuff
    };

    std::vector<std::string> columnTypes = {"V", "V", "V", "V", 
        "V", "V", //6 from common time stuff
        "V","V","V","V", "V","V","V","V", "V","V","V","V", "V","V","V","V", //16
        "V","V","V","V", "V","V","V","V", "V","V","V","V", "V","V","V","V", //32
        "V","V","V","V", "V","V","V","V", "V","V","V","V", "V","V","V","V", //48
        "V","V","V","V" //52 + 6
        };
    std::string combinedColumnTypes;    
    for (const auto& type : columnTypes) {
        combinedColumnTypes += type;  // Concatenate each string in columnTypes
    }
    std::vector<std::string> columnUnits = {"DATE", "s", "s", "s", "s", "s", "", 
    "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN",
    "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN",
    "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN", "DN","DN","DN","DN",
    "DN","DN","DN","DN"};

    int32_t n = SHK_INTEGRATIONS_PER_FILE;
    std::vector<int> columnLengths = {1,1,1,1,1,1,
        n,n,n,n, n,n,n,n, n,n,n,n, n,n,n,n,
        n,n,n,n, n,n,n,n, n,n,n,n, n,n,n,n,
        n,n,n,n, n,n,n,n, n,n,n,n, n,n,n,n,
        n,n,n,n
    };

    //copy data
    struct DataRow {
        uint32_t yyyydoy;
        uint32_t sod;
        uint32_t tai_time_seconds;
        uint32_t tai_time_subseconds;
        uint32_t rec_tai_seconds;
        uint32_t rec_tai_subseconds;
	    //uint32_t spare0; 						// 0
    	uint32_t FPGA_Board_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t FPGA_Board_p5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t FPGA_Board_p3_3_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t FPGA_Board_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t FPGA_Board_p1_2_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p24_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];				// 10
    	uint32_t MEGSA_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p24_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p15_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_m15_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	// 20
    	uint32_t MEGSA_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_Integration_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_Analog_Mux_Register[SHK_INTEGRATIONS_PER_FILE]; // DN
    	uint32_t MEGSA_Digital_Status_Register[SHK_INTEGRATIONS_PER_FILE]; // DN
    	uint32_t MEGSA_Integration_Timer_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_Command_Error_Count_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSA_CEB_FPGA_Version_Register[SHK_INTEGRATIONS_PER_FILE];
    	//uint32_t spare28; // use (adcValue-0x8000) * 305.2 microVolts
    	//uint32_t spare29;
    	//uint32_t spare30;						// 30
    	//uint32_t spare31;
    	uint32_t MEGSB_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p24_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];			// 40
    	uint32_t MEGSB_p24_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p15_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_m15_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	
    	uint32_t MEGSB_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_Integration_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_Analog_Mux_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_Digital_Status_Register[SHK_INTEGRATIONS_PER_FILE];		// 50
    	uint32_t MEGSB_Integration_Timer_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_Command_Error_Count_Register[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_CEB_FPGA_Version_Register[SHK_INTEGRATIONS_PER_FILE];                    

    	uint32_t MEGSA_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];  //special conversion
	    uint32_t MEGSA_PRT[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSB_PRT[SHK_INTEGRATIONS_PER_FILE];

    	uint32_t ESP_Electrometer_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t ESP_Detector_Temperature[SHK_INTEGRATIONS_PER_FILE];
    	uint32_t MEGSP_Temperature[SHK_INTEGRATIONS_PER_FILE]; 			// 60

    } __attribute__((packed));

    // populate the scalars
    DataRow row = {
        SHKStructure.yyyydoy, SHKStructure.sod, SHKStructure.tai_time_seconds,
        SHKStructure.tai_time_subseconds, SHKStructure.rec_tai_seconds,
        SHKStructure.rec_tai_subseconds
    };
    // populate the arrays
    for (size_t i = 0; i < SHK_INTEGRATIONS_PER_FILE; ++i) {
        row.FPGA_Board_Temperature[i] = SHKStructure.FPGA_Board_Temperature[i];
        row.FPGA_Board_p5_0_Voltage[i] = SHKStructure.FPGA_Board_p5_0_Voltage[i];
        row.FPGA_Board_p3_3_Voltage[i] = SHKStructure.FPGA_Board_p3_3_Voltage[i];
        row.FPGA_Board_p2_5_Voltage[i] = SHKStructure.FPGA_Board_p2_5_Voltage[i];
        row.FPGA_Board_p1_2_Voltage[i] = SHKStructure.FPGA_Board_p1_2_Voltage[i];

        row.MEGSA_CEB_Temperature[i] = SHKStructure.MEGSA_CEB_Temperature[i];
        row.MEGSA_CPR_Temperature[i] = SHKStructure.MEGSA_CPR_Temperature[i];

        row.MEGSA_p24_Voltage[i]          = SHKStructure.MEGSA_p24_Voltage[i];
        row.MEGSA_p15_Voltage[i]          = SHKStructure.MEGSA_p15_Voltage[i];
        row.MEGSA_m15_Voltage[i]          = SHKStructure.MEGSA_m15_Voltage[i];
        row.MEGSA_p5_0_Analog_Voltage[i]  = SHKStructure.MEGSA_p5_0_Analog_Voltage[i];
        row.MEGSA_m5_0_Voltage[i]         = SHKStructure.MEGSA_m5_0_Voltage[i];
        row.MEGSA_p5_0_Digital_Voltage[i] = SHKStructure.MEGSA_p5_0_Digital_Voltage[i];
        row.MEGSA_p2_5_Voltage[i]         = SHKStructure.MEGSA_p2_5_Voltage[i];

        row.MEGSA_p24_Current[i]          = SHKStructure.MEGSA_p24_Current[i];
        row.MEGSA_p15_Current[i]          = SHKStructure.MEGSA_p15_Current[i];
        row.MEGSA_m15_Current[i]          = SHKStructure.MEGSA_m15_Current[i];
        row.MEGSA_p5_0_Analog_Current[i]  = SHKStructure.MEGSA_p5_0_Analog_Current[i];
        row.MEGSA_m5_0_Current[i]         = SHKStructure.MEGSA_m5_0_Current[i];
        row.MEGSA_p5_0_Digital_Current[i] = SHKStructure.MEGSA_p5_0_Digital_Current[i];
        row.MEGSA_p2_5_Current[i]         = SHKStructure.MEGSA_p2_5_Current[i];

        row.MEGSA_Integration_Register[i]         = SHKStructure.MEGSA_Integration_Register[i];
        row.MEGSA_Analog_Mux_Register[i]          = SHKStructure.MEGSA_Analog_Mux_Register[i];
        row.MEGSA_Digital_Status_Register[i]      = SHKStructure.MEGSA_Digital_Status_Register[i];
        row.MEGSA_Integration_Timer_Register[i]   = SHKStructure.MEGSA_Integration_Timer_Register[i];
        row.MEGSA_Command_Error_Count_Register[i] = SHKStructure.MEGSA_Command_Error_Count_Register[i];
        row.MEGSA_CEB_FPGA_Version_Register[i]    = SHKStructure.MEGSA_CEB_FPGA_Version_Register[i];

        row.MEGSB_CEB_Temperature[i] = SHKStructure.MEGSB_CEB_Temperature[i];
        row.MEGSB_CPR_Temperature[i] = SHKStructure.MEGSB_CPR_Temperature[i];

        row.MEGSB_p24_Voltage[i]          = SHKStructure.MEGSB_p24_Voltage[i];
        row.MEGSB_p15_Voltage[i]          = SHKStructure.MEGSB_p15_Voltage[i];
        row.MEGSB_m15_Voltage[i]          = SHKStructure.MEGSB_m15_Voltage[i];
        row.MEGSB_p5_0_Analog_Voltage[i]  = SHKStructure.MEGSB_p5_0_Analog_Voltage[i];
        row.MEGSB_m5_0_Voltage[i]         = SHKStructure.MEGSB_m5_0_Voltage[i];
        row.MEGSB_p5_0_Digital_Voltage[i] = SHKStructure.MEGSB_p5_0_Digital_Voltage[i];
        row.MEGSB_p2_5_Voltage[i]         = SHKStructure.MEGSB_p2_5_Voltage[i];

        row.MEGSB_p24_Current[i]          = SHKStructure.MEGSB_p24_Current[i];
        row.MEGSB_p15_Current[i]          = SHKStructure.MEGSB_p15_Current[i];
        row.MEGSB_m15_Current[i]          = SHKStructure.MEGSB_m15_Current[i];
        row.MEGSB_p5_0_Analog_Current[i]  = SHKStructure.MEGSB_p5_0_Analog_Current[i];
        row.MEGSB_m5_0_Current[i]         = SHKStructure.MEGSB_m5_0_Current[i];
        row.MEGSB_p5_0_Digital_Current[i] = SHKStructure.MEGSB_p5_0_Digital_Current[i];
        row.MEGSB_p2_5_Current[i]         = SHKStructure.MEGSB_p2_5_Current[i];

        row.MEGSB_Integration_Register[i]         = SHKStructure.MEGSB_Integration_Register[i];
        row.MEGSB_Analog_Mux_Register[i]          = SHKStructure.MEGSB_Analog_Mux_Register[i];
        row.MEGSB_Digital_Status_Register[i]      = SHKStructure.MEGSB_Digital_Status_Register[i];
        row.MEGSB_Integration_Timer_Register[i]   = SHKStructure.MEGSB_Integration_Timer_Register[i];
        row.MEGSB_Command_Error_Count_Register[i] = SHKStructure.MEGSB_Command_Error_Count_Register[i];
        row.MEGSB_CEB_FPGA_Version_Register[i]    = SHKStructure.MEGSB_CEB_FPGA_Version_Register[i];

        row.MEGSA_Thermistor_Diode[i] = SHKStructure.MEGSA_PRT[i];
        row.MEGSA_PRT[i] = SHKStructure.MEGSA_PRT[i];
        row.MEGSB_Thermistor_Diode[i] = SHKStructure.MEGSB_PRT[i];
        row.MEGSB_PRT[i] = SHKStructure.MEGSB_PRT[i];

        row.ESP_Electrometer_Temperature[i] = SHKStructure.ESP_Electrometer_Temperature[i];
        row.ESP_Detector_Temperature[i] = SHKStructure.ESP_Detector_Temperature[i];
        row.MEGSP_Temperature[i] = SHKStructure.MEGSP_Temperature[i];

    }

    std::vector<uint8_t> data(sizeof(DataRow));
    memcpy(data.data(), &row, sizeof(DataRow));

    return writeBinaryTable(
        filename, data.data(), columnNames.size(), columnNames,
        combinedColumnTypes, columnLengths, columnUnits, true, extname.c_str()
    );
}


// SHK main writer
bool FITSWriter::writeSHKFITS( const SHK_PACKET& SHKStructure) {

    std::cout << "writing SHK FITS file for APID: " << HK_APID << std::endl;
    LogFileWriter::getInstance().logInfo("writing SHK FITS file");

    int32_t status = 0;
    std::string filename = createFITSFilename(HK_APID, SHKStructure.tai_time_seconds);

    fitsfile* fptr = nullptr;
    if (!initializeFITSFile(filename, fptr)) {
        std::cerr << "ERROR: Failed to initialize FITS file: " << filename << std::endl;
        LogFileWriter::getInstance().logError("FITSWriter::writeSHKFITS: Failed to initialize FITS file: {}", filename);
        return false;
    }

    checkFitsStatus(status);

    int result = writeSHKFITSBinaryTable(filename, SHKStructure);
    if (result != 0) {
        std::cerr << "Failed to write binary table to SHK FITS file: " << filename << std::endl;
        return false;
    }

    // this is where to write the converted SHK data into a second HDU

    std::cout << "FITSWriter::writeSHKFITS successfully wrote " << filename << std::endl;
    LogFileWriter::getInstance().logInfo("FITSWriter::writeSHKFITS successfully wrote {}", filename);

    return true;

}

