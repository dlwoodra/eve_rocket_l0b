#ifndef USB_INPUT_SOURCE_HPP
#define USB_INPUT_SOURCE_HPP

#include "InputSource.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <termios.h>
#include <stdio.h>
#include "okFrontPanelDLL.h"

// *********************************************************************/
// Settings
// *********************************************************************/
#define MAX_DEAD_TIME_MS     200

// *********************************************************************/
// Globals
// *********************************************************************/

// pointer to front panel object
OpalKelly::FrontPanelDevices devices;
OpalKelly::FrontPanelPtr devptr;
okCFrontPanel *dev;
okTDeviceInfo  m_devInfo;

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
    USBInputSource(const std::string& devicePath); // : portName(portName), isOpen(false) {}
    ~USBInputSource(); // { close(); }

    bool open() override {
        // Implement USB port opening logic here
        //isOpen = true;  // Example, change to actual open status
        //return isOpen;
    }

    void close() override {
        // Implement USB port closing logic here
        //isOpen = false;
    }

    bool read(uint8_t* buffer, size_t size) override {
        // Implement USB port read logic here
        //return true; // Example, change to actual read status
    }

    bool isOpen() const override {
        //return isOpen;
    }

private:
    int usbHandle;
    std::string devicePath;
    //bool isOpen;
};

#endif // USB_INPUT_SOURCE_HPP
