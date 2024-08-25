#include "FileInputSource.hpp"

FileInputSource::FileInputSource(const std::string& filename) 
    : filename(filename) {}

FileInputSource::~FileInputSource() {
    close();
}

bool FileInputSource::open() {
    fileStream.open(filename, std::ios::binary);
    return fileStream.is_open();
}

void FileInputSource::close() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

bool FileInputSource::read(uint8_t* buffer, size_t size) {
    if (isOpen()) {
        fileStream.read(reinterpret_cast<char*>(buffer), size);
        return fileStream.gcount() == static_cast<std::streamsize>(size);
    }
    return false;
}

bool FileInputSource::isOpen() const {   // Implementing isOpen method
    return fileStream.is_open();
}
