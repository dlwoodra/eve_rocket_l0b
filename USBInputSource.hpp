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

    // this is soe we can test selectUSBSerialNumber
    std::string getSerialNumber() {
        return selectUSBSerialNumber();
    }

private:
    
    std::string selectUSBSerialNumber();
    void initializeGSE();
    void processTransmit();
    void processReceive();
    //void checkForGSECommand();
    void checkLinkStatus();
    void setGSERegister(int addr, unsigned char data);
    unsigned short readGSERegister(int addr);
    void resetInterface();

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
