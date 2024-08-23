#ifndef FILE_INPUT_SOURCE_HPP
#define FILE_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include <fstream>

class FileInputSource : public InputSource {
public:
    FileInputSource(const std::string& filename) : filename(filename) {}
    ~FileInputSource() { close(); }

    bool open() override {
        fileStream.open(filename, std::ios::binary);
        return fileStream.is_open();
    }

    void close() override {
        if (fileStream.is_open()) {
            fileStream.close();
        }
    }

    bool read(uint8_t* buffer, size_t size) override {
    if (isOpen()) {
        fileStream.read(reinterpret_cast<char*>(buffer), size);
        return fileStream.gcount() == static_cast<std::streamsize>(size);
        }
        return false;
    }    

    bool isOpen() const override {
        return fileStream.is_open();
    }

private:
    std::ifstream fileStream;
    std::string filename;
};

#endif // FILE_INPUT_SOURCE_HPP
