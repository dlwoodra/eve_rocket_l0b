#ifndef FILE_INPUT_SOURCE_HPP
#define FILE_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include <fstream>
#include <string>
#include <zlib.h>

class FileInputSource : public InputSource {
public:
    FileInputSource(const std::string& filename);
    ~FileInputSource();

    bool open() override;
    void close() override;
    bool read(uint8_t* buffer, size_t size) override;
    bool isOpen() const override;

private:
    std::ifstream fileStream; //for uncompressed files
    gzFile gzFileStream = nullptr; //for compressed files
    std::string filename;
    bool isCompressed = false;
    
    bool checkIfCompressed() const;
};

#endif // FILE_INPUT_SOURCE_HPP
