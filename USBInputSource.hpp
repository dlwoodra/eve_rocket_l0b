#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include "TimeInfo.hpp"
#include <cstdio>
#include <string>
#include <stdio.h>
#include <okFrontPanelDLL.h>

class USBInputSource : public InputSource {
public:
    USBInputSource(const std::string& serialNumber);
    //: serialNumber(serialNumber), pFileCommand(nullptr), pFileTelemetry(nullptr), 
    //  telemetryOpen(false), commandOpen(false), continueProcessing(false), 
    //  ctrTxBytes(0), ctrRxBytes(0), commandBytesLeft(0), dev(nullptr) {}
    //USBInputSource(const std::string& serialNumber ) : serialNumber(serialNumber) {} //, dev(nullptr), opened(false) {}
    //USBInputSource();
    ~USBInputSource(); // { close(); }

    //bool open() override;
    //void close() override;
    //size_t read(char* buffer, size_t maxSize) override;
    //bool isOpen() const override;

    bool open() override; // {
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

    void close() override; // {
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

    bool isOpen() const override; // {
        //return isOpen;
        //return opened;
    //}

private:
    
    void initializeGSE();
    void processTransmit();
    void processReceive();
    void checkForGSECommand();
    void checkLinkStatus();
    void setGSERegister(int addr, unsigned char data);
    unsigned short readGSERegister(int addr);

    std::string serialNumber;



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
