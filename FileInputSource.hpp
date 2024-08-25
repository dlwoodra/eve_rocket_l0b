#ifndef FILE_INPUT_SOURCE_HPP
#define FILE_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include <fstream>

class FileInputSource : public InputSource {
public:
    FileInputSource(const std::string& filename);
    ~FileInputSource();

    bool open() override;

    void close() override;

    bool read(uint8_t* buffer, size_t size) override;

    bool isOpen() const override;

private:
    std::ifstream fileStream;
    std::string filename;
};

#endif // FILE_INPUT_SOURCE_HPP
