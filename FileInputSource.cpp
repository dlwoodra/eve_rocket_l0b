#include "FileInputSource.hpp"

FileInputSource::FileInputSource(const std::string& filename) 
    : filename(filename), isCompressed(checkIfCompressed()) {}

FileInputSource::~FileInputSource() {
    close();
}

bool FileInputSource::checkIfCompressed() const {
    // Check if filename ends with ".gz" (case sensitive)
    return filename.size() >= 3 && filename.substr(filename.size() - 3) == ".gz";
}

bool FileInputSource::open() {
    if (isCompressed) {
        gzFileStream = gzopen(filename.c_str(), "rb"); // Open gzip file
        return gzFileStream != nullptr;
    } else {
        fileStream.open(filename, std::ios::binary); // Open regular file
        return fileStream.is_open();
    }
}

void FileInputSource::close() {
    if (isCompressed) {
        if (gzFileStream) {
            gzclose(gzFileStream);
            gzFileStream = nullptr;
        }
    } else {
        if (fileStream.is_open()) {
            fileStream.close();
        }
    }
}

bool FileInputSource::read(uint8_t* buffer, size_t size) {
    if (isOpen()) {
        if (isCompressed) {
            if (gzFileStream) {
                int bytesRead = gzread(gzFileStream, buffer, size);
                return bytesRead == static_cast<int>(size);
            }
        } else {
            if (fileStream.is_open()) {
                fileStream.read(reinterpret_cast<char*>(buffer), size);
                return fileStream.gcount() == static_cast<std::streamsize>(size);
            }
        }
    }
    return false;
}

bool FileInputSource::isOpen() const {   // Implementing isOpen method
    return isCompressed ? (gzFileStream != nullptr) : fileStream.is_open();
}
