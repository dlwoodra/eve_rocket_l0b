#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "CCSDSReader.hpp"
#include "InputSource.hpp"
#include "TimeInfo.hpp"
#include <cstdint>
#include <cstdio>
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

    //bool open() override; // {
        // Implement USB port opening logic here
        //isOpen = true;  // Example, change to actual open status
        //return isOpen;
        //dev = new okCFrontPanel();
        //if (dev->OpenBySerial(serialNumber) != okCFrontPanel::NoError) {
            //std::cerr << "Failed to open Opal Kelly device" << std::endl;
            //delete dev;
            //dev = nullptr;
            //return false;
        //}
        //opened = true;
        //return true;
    //}

    //void close() override; // {
        // Implement USB port closing logic here
        //isOpen = false;
        //if(dev) {
            //delete dev;
            //dev = nullptr;
        //}
        //opened = false;
    //}

    bool read(uint8_t* buffer, size_t size) override;
    //{
        // Implement USB port read logic here
        //return true; // Example, change to actual read status
        //if (!dev) return false;
        //long transferred = dev->ReadFromBlockPipeOut(0xA0, 1024, size, buffer);
        //return transferred == size;
    //}

    // this is so we can test selectUSBSerialNumber
    std::string getSerialNumber() {
        return selectUSBSerialNumber();
    }

    void ProcRx();
    void CGProcRx(void);

private:
    
    std::string selectUSBSerialNumber();
    void initializeGSE();
    void processTransmit();
    void processReceive();
    //void checkForGSECommand();
    void checkLinkStatus();
    void setGSERegister(int addr, unsigned char data);
    unsigned short readGSERegister(int addr);
    void resetInterface(int32_t milliSeconds);
    void powerOnLED();
    void powerOffLED();

    std::ofstream initializeOutputFile();
    bool isReceiveFIFOEmpty();
    void handleReceiveFIFOError();
    int32_t readDataFromUSB();

    void GSEprocessBlock(unsigned long* pBlk);
    void GSEprocessPacketHeader(unsigned long*& pBlk, unsigned int& blkIdx, unsigned int& nBlkLeft, int& state, int& APID, unsigned int& pktIdx, unsigned int& nPktLeft);
    void GSEprocessPacketContinuation(unsigned long*& pBlk, unsigned int& blkIdx, unsigned int& nBlkLeft, unsigned int& pktIdx, unsigned int& nPktLeft, int& state);


    // packet lengths are in 32-bit words and do not include sync-code
    static const uint16_t nAPID = 5;
    static const uint16_t LUT_APID[nAPID];
    static const uint16_t LUT_PktLen[nAPID];

    // variables used to persist with the blocks and packet processing
    int state; // current state of the processing state machine
    int APID;
    unsigned int pktIdx; // an index into the packet buffer to join packets spread across 2 blocks
    unsigned int nPktLeft; // number of remaining words in the packet

    // flags
    int flgTelOpen;
    int flgCommandOpen;
    int flgSendCommand;

    // states
    int PPState;

    // buffers
    unsigned long RxBuff[16384];
    unsigned long PktBuff[4096];
    unsigned long PktNull[4096];

    int GSEType;
    long ctrTxPkts;
    long ctrRxPkts;
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
    int gseType;
    long ctrTxBytes;
    long ctrRxBytes;
    long commandBytesLeft;
    TimeInfo openRxTime;
    TimeInfo lastRxTime;

    const int MAX_DEAD_TIME_MS = 200;

    //void* dev;
    //bool opened;
    
    OpalKelly::FrontPanelDevices devices;
    OpalKelly::FrontPanelPtr devptr;
    okCFrontPanel* dev;
    okTDeviceInfo m_devInfo;

};

#endif // USB_INPUT_SOURCE_HPP
