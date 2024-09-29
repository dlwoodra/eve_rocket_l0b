#ifndef FITSWRITER_HPP
#define FITSWRITER_HPP

#include "eve_l0b.hpp" 
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

#include "/usr/local/include/fitsio.h"

class FITSWriter {
public:
    FITSWriter();
    ~FITSWriter();

    // interfaces to common writeMegsFITS
    bool writeMegsAFITS(const MEGS_IMAGE_REC& megsStructure);
    bool writeMegsBFITS(const MEGS_IMAGE_REC& megsStructure);
    // interfaces to write the other packet types
    bool writeMegsPFITS(const MEGSP_PACKET& megsPStructure);
    bool writeESPFITS(const ESP_PACKET& ESPStructure);
    bool writeSHKFITS(const SHK_PACKET& SHKStructure);

private:
    // common MEGS image writing
    bool writeMegsFITS(const MEGS_IMAGE_REC& megsStructure, uint16_t apid, const std::string& extname);

    // common function to create a FITS filename based on APID and timestamp
    // that calls the cfitsio API fits_create_file function
    std::string createFITSFilename(uint16_t apid, double timestamp);

    // common function to initialize a FITS file
    bool initializeFITSFile(const std::string& filename, fitsfile*& fptr);

    //handler for errors - all errors are fatal
    void checkFitsStatus(int status);

    // converts a vector of strings to a vector of char* for arays of FITS types
    std::vector<char*> convertToCharPtrArray(const std::vector<std::string>& vec);

    // converts an array of strings into a vector of char* for scalar FITS types
    // this might be replacable by convertToCharPtrArray, relies on new
    std::vector<char*> convertTypesToCharPtrArray(const std::string& types);
    std::vector<char*> convertTypesToTFormPtrArray(const std::string& types, const std::vector<int>& lengths);
    // use to free memory allocated in convertTypesTo... functions
    void freeCharPtrArray(std::vector<char*>& array);

    bool writeMegsFITSImgHeader(fitsfile* fptr, const MEGS_IMAGE_REC& megsStructure, int32_t& status);
    int writeBinaryTable(const std::string& filename, const void* data, int columns, 
        const std::vector<std::string>& names, const std::string& types, const std::vector<int>& columnLengths,
        const std::vector<std::string>& units, bool useUnits, const std::string& extname);
    int writeMegsFITSBinaryTable(const std::string& filename, const MEGS_IMAGE_REC& megsStructure,
        const std::string& extname, uint16_t apid);    
    int writeMegsPFITSBinaryTable(const std::string& filename, const MEGSP_PACKET& megsPStructure);
    int writeESPFITSBinaryTable(const std::string& filename, const ESP_PACKET& ESPStructure);
    int writeSHKFITSBinaryTable(const std::string& filename, const SHK_PACKET& SHKStructure);
};

#endif // FITSWRITER_HPP

