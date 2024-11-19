#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "byteswap.hpp"
#include <csignal>
#include "CCSDSReader.hpp"
#include "InputSource.hpp"
#include "LogFileWriter.hpp"
#include "RecordFileWriter.hpp"
#include "TimeInfo.hpp"
#include "ProgramState.hpp"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <stdio.h>
#include <okFrontPanelDLL.h>

// these are used to implement Windows Sleep
#include <chrono>
#include <thread>

constexpr uint32_t wordsInFullBuffer = 16384; //32-bit words
constexpr uint32_t bytesInFullBuffer = 16384 * 4; //bytes
constexpr uint32_t maxPacketWords = 443; //32-bit words
constexpr uint32_t maxPacketBytes = 443 * 4; //bytes
constexpr uint32_t lengthInWordsOfStrippedRxBuff = wordsInFullBuffer + maxPacketWords;


class USBInputSource : public InputSource {
public:
    USBInputSource(const std::string& serialNumber);
    ~USBInputSource(); // { close(); }

    bool open() override;
    void close() override;
    //size_t read(char* buffer, size_t maxSize) override;
    bool isOpen() const override;

    bool read(uint8_t* buffer, size_t size) override;

    // this is so we can test selectUSBSerialNumber
    std::string getSerialNumber() {
        return selectUSBSerialNumber();
    }

    //std::ofstream outputFile;

    //CCSDSReader usbReader;
    void ProcRx(CCSDSReader& usbReader);
    void CGProcRx(CCSDSReader& usbReader);
    void replaceCGProxRx(CCSDSReader& usbReader);


private:
    void writeBinaryToFile(const std::string& filename, const uint32_t* data, size_t size);
    std::string selectUSBSerialNumber();
    void initializeGSE();
    void processTransmit();
    void processReceive();
    //void checkForGSECommand();
    void checkLinkStatus();
    void setGSERegister(int16_t addr, unsigned char data);
    unsigned short readGSERegister(int addr);
    void resetInterface(int32_t milliSeconds);
    void powerOnLED();
    void powerOffLED();

    std::ofstream initializeOutputFile();
    bool isReceiveFIFOEmpty();
    void handleReceiveFIFOError();
    int32_t readDataFromUSB(const char* capturedFilename = nullptr);
    int32_t copyToPackedBuffer(uint32_t startIndex, uint32_t* strippedRxBuff);

    void discardFirstFourBytes();

    void GSEProcessPacket(uint32_t *PktBuff, uint16_t APID, CCSDSReader& usbReader);

    // packet lengths are in 32-bit words and do not include sync-code
    static const uint16_t nAPID = 5;
    static const uint16_t LUT_APID[nAPID];
    static const uint16_t LUT_PktLen[nAPID];

    // variables used to persist with the blocks and packet processing
    static int16_t state; // current state of the processing state machine
    static int16_t APID;
    static uint16_t pktIdx; // an index into the packet buffer to join packets spread across 2 blocks
    static uint16_t nPktLeft; // number of remaining words in the packet
    uint16_t APIDidx;

    // sizes

    // buffers
    uint32_t RxBuff[wordsInFullBuffer];
    // longest packetfield is 0x6e1 or 1761, need to add 6 byte header+1 + 4 byte sync
    // that makes 1761+6+1+4 = 1772 bytes or 443 32-bit words
    // 16384+1772/4 from max MEGS packet length = 16827 to leave margin

    // moved into replaceCGProcRx
    //uint32_t strippedRxBuff[lengthInWordsOfStrippedRxBuff]; //32-bit words

    //uint32_t PktBuff[4096];
    //uint32_t PktNull[4096];

    int16_t GSEType;
    int32_t ctrRxPkts;
    char StatusStr[256]; // reference string to hold status messages

    // the string serialNumber is has the last 4 digits printed on the barcode sticker
    // on the Opal Kelly FPGA integration module
    const std::string serialNumber;

    FILE* pFileCommand;
    FILE* pFileTelemetry;

    bool telemetryOpen;
    bool commandOpen;
    bool continueProcessing;

    uint8_t rxBuffer[bytesInFullBuffer]; //65536 bytes
    int16_t gseType;
    int32_t ctrTxBytes;
    int32_t ctrRxBytes;
    int32_t commandBytesLeft;
    TimeInfo openRxTime;
    TimeInfo lastRxTime;

    const int16_t MAX_DEAD_TIME_MS = 200;

    OpalKelly::FrontPanelDevices devices;
    OpalKelly::FrontPanelPtr devptr;
    okCFrontPanel* dev;
    okTDeviceInfo m_devInfo;

    std::unique_ptr<RecordFileWriter> recordFileWriter;

};

#endif // USB_INPUT_SOURCE_HPP
