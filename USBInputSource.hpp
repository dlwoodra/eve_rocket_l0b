#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <termios.h>
#include <stdio.h>
//#include "okFrontPanelDLL.h"

// *********************************************************************/
// Settings
// *********************************************************************/
//#define MAX_DEAD_TIME_MS     200

// *********************************************************************/
// Globals
// *********************************************************************/

// pointer to front panel object
//OpalKelly::FrontPanelDevices devices;
//OpalKelly::FrontPanelPtr devptr;
//okCFrontPanel *dev;
//okTDeviceInfo  m_devInfo;

// flags
int flgTelemetryOpen;
int flgCommandOpen;
int flgSendCommand;

// buffers
unsigned char RxBuff[65536];

// continue flag
int flgCont;

// status
int GSEType;
long ctrTxBytes;
long ctrRxBytes;
long CommandBytesLeft;
//SYSTEMTIME OpenRxTime;
//SYSTEMTIME LastRxTime;

// *********************************************************************/
// Function Prototypes
// *********************************************************************/
unsigned short CGInit(void);
void CGProcTx(void);
void CGProcRx(void);
void CheckForGSECommand(void);
void CheckLinkStatus(void);
void SetGSERegister(int addr, unsigned char data);
unsigned short ReadGSERegister(int addr);

class USBInputSource : public InputSource {
public:
    USBInputSource(const std::string& serialNumber ) : serialNumber(serialNumber), dev(nullptr), opened(false) {}
    ~USBInputSource() { close(); }

    bool open() override {
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
        opened = true;
        return true;
    }

    void close() override {
        // Implement USB port closing logic here
        //isOpen = false;
        //if(dev) {
            //delete dev;
            //dev = nullptr;
        //}
        opened = false;
    }

    bool read(uint8_t* buffer, size_t size) override {
        // Implement USB port read logic here
        //return true; // Example, change to actual read status
        //if (!dev) return false;
        //long transferred = dev->ReadFromBlockPipeOut(0xA0, 1024, size, buffer);
        //return transferred == size;
    }

    bool isOpen() const override {
        //return isOpen;
        return opened;
    }

private:
    //int usbHandle;
    //okCFrontPanel* dev;
    // for testing define dev as a pointer
    void* dev;
    std::string serialNumber;
    bool opened;
};

#endif // USB_INPUT_SOURCE_HPP
