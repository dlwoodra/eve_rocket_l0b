#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "CCSDSReader.hpp"
#include "InputSource.hpp"
#include "LogFileWriter.hpp"
#include "RecordFileWriter.hpp"
#include "TimeInfo.hpp"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <stdio.h>
#include <okFrontPanelDLL.h>

// these are used to implement Windows Sleep
#include <chrono>
#include <thread>

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

    void ProcRx();
    void CGProcRx(void);

private:
    
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
    int32_t readDataFromUSB();

    void GSEprocessBlock(uint32_t * pBlk);
    void GSEprocessPacketHeader(uint32_t *& pBlk, uint16_t & blkIdx, uint16_t & nBlkLeft, int16_t & state, int16_t & APID, uint16_t& pktIdx, uint16_t & nPktLeft);
    void GSEprocessPacketContinuation(uint32_t *& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, uint16_t & pktIdx, uint16_t & nPktLeft, int16_t & state);
    void GSEContinuePacket(uint32_t *& pBlk, uint16_t &blkIdx, uint16_t& nBlkLeft, int16_t& state, uint16_t& pktIdx, uint16_t& nPktLeft, int16_t APIDidx);

    // packet lengths are in 32-bit words and do not include sync-code
    static const uint16_t nAPID = 5;
    static const uint16_t LUT_APID[nAPID];
    static const uint16_t LUT_PktLen[nAPID];

    // variables used to persist with the blocks and packet processing
    int16_t state; // current state of the processing state machine
    int16_t APID;
    uint16_t pktIdx; // an index into the packet buffer to join packets spread across 2 blocks
    uint16_t nPktLeft; // number of remaining words in the packet
    uint16_t APIDidx;
    //uint16_t blkSize;

    // flags
    int flgTelOpen;
    int flgCommandOpen;
    int flgSendCommand;

    // states
    int PPState;

    // buffers
    uint32_t RxBuff[16384];
    uint32_t PktBuff[4096];
    uint32_t PktNull[4096];

    int16_t GSEType;
    int32_t ctrTxPkts;
    int32_t ctrRxPkts;
    //long CommandBytesLeft;
    char StatusStr[256]; // reference string to hold status messages

    // the string serialNumber is has the last 4 digits printed on the barcode sticker
    // on the Opal Kelly FPGA integration module
    const std::string serialNumber;

    FILE* pFileCommand;
    FILE* pFileTelemetry;

    bool telemetryOpen;
    bool commandOpen;
    bool continueProcessing;

    unsigned char rxBuffer[65536];
    int16_t gseType;
    int32_t ctrTxBytes;
    int32_t ctrRxBytes;
    int32_t commandBytesLeft;
    TimeInfo openRxTime;
    TimeInfo lastRxTime;

    const int16_t MAX_DEAD_TIME_MS = 200;

    //void* dev;
    //bool opened;
    
    OpalKelly::FrontPanelDevices devices;
    OpalKelly::FrontPanelPtr devptr;
    okCFrontPanel* dev;
    okTDeviceInfo m_devInfo;

    std::unique_ptr<RecordFileWriter> recordFileWriter;

};

#endif // USB_INPUT_SOURCE_HPP
