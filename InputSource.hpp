#ifndef INPUT_SOURCE_HPP
#define INPUT_SOURCE_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool read(uint8_t* buffer, size_t size) = 0;
    virtual bool isOpen() const = 0;
};

#endif // INPUT_SOURCE_HPP
