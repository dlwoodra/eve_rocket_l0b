#include <okFrontPanelDLL.h>
#include <iostream>

int main() {

    OpalKelly::FrontPanelDevices devices;
    OpalKelly::FrontPanelPtr devptr;
    okCFrontPanel* dev;
    okTDeviceInfo m_devInfo;

    devptr = devices.Open(""); // get the first connected device

    dev = devptr.get();
    if (!dev) {
        int deviceCount = devices.GetCount();
        if (deviceCount > 1) {
            std::cout << "ERROR: initializeGSE: Multiple devices found!" << std::endl;
            //LogFileWriter::getInstance().logError("initializeGSE Multiple devices found!");
        }
        if (deviceCount == 0) { // if more than 1, GetCount returns the number
            std::cout << "ERROR: No connected devices detected." << std::endl;
            //LogFileWriter::getInstance().logError("initializeGSE No connected devices found!");
        } else {
            std::cout << "ERROR: Device could not be opened." << std::endl;
            //LogFileWriter::getInstance().logError("initializeGSE Device could not be opened!");
        }
        return -1;
    }
    dev->GetDeviceInfo(&m_devInfo);

    // Alan loads the GSE firmware from a file
    // this is the one David gave me
    dev->ConfigureFPGA("firmware/hss_usb_fpga_2024_08_28.bit");

    if (!dev) {
        std::cout << "ERROR code received: "<< dev << std::endl;
        return -1;
    }
    std::cout << "Successfully programmed the FPGA" << std::endl;

    return 0;
}
