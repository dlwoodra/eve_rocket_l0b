#ifndef FITSWRITER_HPP
#define FITSWRITER_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

#include "/usr/local/include/fitsio.h"

// Custom deleter for fitsfile* to be used with std::unique_ptr
struct FITSFileDeleter {
    void operator()(fitsfile* fptr) const {
        if (fptr) {
            int status = 0;
            fits_close_file(fptr, &status);
            if (status) {
                fits_report_error(stderr, status);
            }
        }
    }
};

class FITSWriter {
public:
    FITSWriter();
    ~FITSWriter();

    // Write the packet data to the appropriate FITS file based on APID and timestamp
    bool writePacketToFITS(const std::vector<uint8_t>& packet, uint16_t apid, double timestamp);

private:
    // Helper function to create a FITS filename based on APID and timestamp
    std::string createFITSFilename(uint16_t apid, double timestamp);

    // Helper function to initialize a FITS file
    bool initializeFITSFile(const std::string& filename);

    // Helper function to write data to a FITS file
    bool writeDataToFITS(fitsfile* fptr, const std::vector<uint8_t>& data);

    // Map to store FITS file pointers for each APID
    //std::unordered_map<uint16_t, std::unique_ptr<fitsfile, decltype(&fits_close_file)>> fitsFileMap;
    
    // Map to store FITS file pointers for each APID
    std::unordered_map<std::string, std::unique_ptr<fitsfile, FITSFileDeleter>> fitsFileMap;

    // Utility function to close all FITS files
    void closeAllFITSFiles();
};

#endif // FITSWRITER_HPP

