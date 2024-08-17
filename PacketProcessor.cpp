//#include <string>
#include "FITSWriter.hpp"
#include <vector>
#include <chrono>

class PacketProcessor {
public:
    virtual ~PacketProcessor() = default;
    virtual void processPacket(const std::vector<uint8_t>& packet) = 0;

protected:
    std::string generateFilename(const std::chrono::system_clock::time_point& timestamp);
    void writeToFile(const std::string& filename, const std::vector<uint8_t>& data);
};

class APID601_602Processor : public PacketProcessor {
public:
    void processPacket(const std::vector<uint8_t>& packet) override;
private:
    void organizePixels(const std::vector<uint8_t>& packet);
    void displayImage();
    void writeImageToFile(const std::string& filename);
};

class APID604Processor : public PacketProcessor {
public:
    void processPacket(const std::vector<uint8_t>& packet) override;
    // Specific methods for APID 604 processing
};

class APID605Processor : public PacketProcessor {
public:
    void processPacket(const std::vector<uint8_t>& packet) override;
    // Specific methods for APID 605 processing
};
