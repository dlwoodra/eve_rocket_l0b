#include "USBInputSource.hpp"
#include "RecordFileWriter.hpp"

// constructor implementation
    USBInputSource::USBInputSource(const std::string& serialNumber ) 
      : serialNumber(selectUSBSerialNumber()), 
        pFileCommand(nullptr), 
        pFileTelemetry(nullptr), 
        telemetryOpen(false), 
        commandOpen(false), 
        continueProcessing(false), 
        ctrTxBytes(0), 
        ctrRxBytes(0), 
        commandBytesLeft(0), 
        dev(nullptr) {

        std::cout << "USBInputSource initialized with serial number: " << serialNumber << std::endl;
      }

USBInputSource::~USBInputSource() {
    close();
}

bool USBInputSource::open() {
    std::cout << "Opening USBInputSource for serial number: " << serialNumber << std::endl;

    initializeGSE();
    continueProcessing = true;

    return dev != nullptr;
}

void USBInputSource::close() {
    // do we need pFileTelemetry and pFileCommand?
    if (pFileTelemetry) {
        fclose(pFileTelemetry);
        pFileTelemetry = nullptr;
    }

    if (pFileCommand) {
        fclose(pFileCommand);
        pFileCommand = nullptr;
    }

    continueProcessing = false;
    std::cout << "USB input source closed." << std::endl;
}

bool USBInputSource::read(uint8_t* buffer, size_t maxSize) {
    size_t bytesRead = dev->ReadFromBlockPipeOut(0xA3, 1024, maxSize, reinterpret_cast<unsigned char*>(buffer));
    return bytesRead == maxSize;
    //processReceive();
    //return 0; // Modify as needed to return actual read size
}

bool USBInputSource::isOpen() const {
    return continueProcessing;
}

std::string USBInputSource::selectUSBSerialNumber() {

    // the string serialNumber is has the last 4 digits printed on the barcode sticker
    // on the Opal Kelly FPGA integration module
    std::string serialNumber = "12345678"; //replace after probing the HW

    okCFrontPanel dev;
    int devCount = dev.GetDeviceCount();

    std::cout << "selectUSBSerialNumber: Number of Opal Kelly devices found: " << devCount << std::endl;
    if ( devCount == 0 ) {
        std::cout << "ERROR: Fatal - no Opal Kelly devices found - cannot continue" << std::endl;
        std::cout << " *** CONTINUING FOR DEBUGGING PURPOSES ***" << std::endl;
    } else {
        for (int i=0; i < devCount; i++) {
	        std::cout << "selectUSBSerialNumber: Device[" << i << "] Model : " << dev.GetDeviceListModel(i) << "\n";
	        std::cout << "selectUSBSerialNumber: Device[" << i << "] Serial : " << dev.GetDeviceListSerial(i) << "\n";
        }
        std::cout << "selecting first USBSerial device " << std::endl;
        serialNumber = dev.GetDeviceListSerial(0); // choosing first one found
    }
    return serialNumber;
}

void USBInputSource::initializeGSE() {
    devptr = devices.Open(""); // get the first connected device
    dev = devptr.get();
    if (!dev) {
        if (devices.GetCount() > 1) {
            std::cout << "ERROR: initializeGSE: Multiple devices found!" << std::endl;
        }
        if (!devices.GetCount()) { // if more than 1, GetCount returns the number
            std::cout << "ERROR: No connected devices detected." << std::endl;
        } else {
            std::cout << "ERROR: Device could not be opened." << std::endl;
        }
        std::cout << "FATAL: terminating" << std::endl;
        std::exit(EXIT_FAILURE);
        //return;
    }

    dev->GetDeviceInfo(&m_devInfo);
    std::cout << "Found a device: " << m_devInfo.productName << std::endl;

    dev->LoadDefaultPLLConfiguration();
    unsigned short firmwareVer = readGSERegister(4); // probably HW specific
    gseType = firmwareVer >> 12; // can this even work?

    switch (gseType) {
        case 1:
            std::cout << "Sync-Serial GSE. Firmware Version: " << std::hex << firmwareVer << std::endl;
            break;
        case 2:
            std::cout << "SpaceWire GSE. Firmware Version: " << std::hex << firmwareVer << std::endl;
            break;
        case 3:
            std::cout << "Parallel GSE. Firmware Version: " << std::hex << firmwareVer << std::endl;
            break;
        default:
            std::cout << "Error. Unrecognized firmware: " << std::hex << firmwareVer << std::endl;
            return;
    }

    resetInterface(); // turn on the light

    std::cout << "GSE initialized." << std::endl;
}

void USBInputSource::resetInterface() {

    // Reset Space interface
    setGSERegister(0, 1);
    setGSERegister(0, 0);

    // Turn on USB LED to show GSE is communicating
    setGSERegister(1, 1);
}

void USBInputSource::processTransmit() {
    unsigned char cmdBuff[32];

    if (!commandOpen) return;

    // Check if transmit FIFO is empty
    if (!(readGSERegister(0) & 0x01)) return;

    // Read data
    if (fread(cmdBuff, 1, 32, pFileCommand) != 32) {
        std::cout << "Command file read error" << std::endl;
        return;
    }

    // Send data to GSE
    if (dev->WriteToPipeIn(0x80, 32, cmdBuff) != 32) {
        std::cout << "Transmit data error" << std::endl;
    }

    // Give transmit command to GSE
    setGSERegister(0, 2);

    ctrTxBytes += 32;
    commandBytesLeft -= 32;
    if (commandBytesLeft < 32) {
        commandBytesLeft = 0;
        fclose(pFileCommand);
        commandOpen = false;
        //DeleteFile(L"CSIE_Command.bin");
    }
}

void USBInputSource::processReceive() {
    unsigned char* pBlkStart = rxBuffer;

    // Check if receive FIFO is empty
    if (readGSERegister(0) & 0x02) return;

    // Get data, 64k at a time
    if (dev->ReadFromBlockPipeOut(0xA3, 1024, 65536, rxBuffer) != 65536) {
        std::cout << "\nAll data not returned" << std::endl;
        return;
    }

    for (int i = 0; i <= 63; ++i) {
        long dataLength = 4 * (*(reinterpret_cast<long*>(pBlkStart)));
        if (dataLength > 1020) {
            std::cout << "\nInvalid amount of data" << std::endl;
            return;
        }

        if (dataLength) {
            if (!telemetryOpen) {
                pFileTelemetry = fopen("CSIE_Telemetry.bin", "wb");
                if (!pFileTelemetry) {
                    std::cout << "Could not open Telemetry file" << std::endl;
                    return;
                }
                telemetryOpen = true;
                ctrRxBytes = 0;
                openRxTime.updateNow();
            }

            pBlkStart += 4;
            fwrite(pBlkStart, 1, dataLength, pFileTelemetry);
            ctrRxBytes += dataLength;
            pBlkStart += 1020;

            lastRxTime.updateNow();
        } else {
            pBlkStart += 1024;
        }
    }
}

//void USBInputSource::checkForGSECommand() {
//    FILE* pFileGSECommand;
//    unsigned char opcode;

//    if (fopen_s(&pFileGSECommand, "CSIE_GSECommand.bin", "rb")) return;

//    if (fread(&opcode, 1, 1, pFileGSECommand) != 1) {
//        opcode = 0;
//    }
//    fclose(pFileGSECommand);
//    DeleteFile(L"CSIE_GSECommand.bin");

//    if (opcode == 1) {
//        pFileCommand = fopen("CSIE_Command.bin", "rb");
//        if (!pFileCommand) {
//            std::cout << "Could not open command file" << std::endl;
//            return;
//        }

//        fseek(pFileCommand, 0, SEEK_END);
//        commandBytesLeft = ftell(pFileCommand);
//        fseek(pFileCommand, 0, SEEK_SET);
//        commandOpen = true;
//    } else {
//        std::cout << "Invalid GSE command opcode" << std::endl;
//    }
//}

void USBInputSource::checkLinkStatus() {
    unsigned short r = readGSERegister(0x02);
    TimeInfo time;
    int msElapsed = 0;

    switch (gseType) {
        case 1:
            std::cout << (r ? "\rERR" : "\rGOOD") << "\t" << commandOpen << "\t" << telemetryOpen
                      << "\t" << ctrTxBytes << "\t" << ctrRxBytes / 1024 << "\t" << std::hex << r;
            break;
        case 2:
            std::cout << (((r & 0xFFFE) != 0x7006) ? "\rDOWN" : "\rGOOD") << "\t" << commandOpen
                      << "\t" << telemetryOpen << "\t" << ctrTxBytes << "\t" << ctrRxBytes / 1024
                      << "\t" << std::hex << r;
            break;
        case 3:
            std::cout << (r ? "\rERR" : "\rGOOD") << "\t" << telemetryOpen << "\t" << ctrRxBytes / 1024
                      << "\t" << std::hex << r;
            break;
        default:
            break;
    }

    if (telemetryOpen) {
        msElapsed = time.calculateTimeDifferenceInMilliseconds(lastRxTime);
        if (msElapsed > MAX_DEAD_TIME_MS) {
            std::cout << "\nTelemetry closed after " << ctrRxBytes << " bytes received.\n";
            telemetryOpen = false;
            fclose(pFileTelemetry);
            pFileTelemetry = nullptr;
        }
    }
}

void USBInputSource::setGSERegister(int addr, unsigned char data) {
    unsigned char buf[2] = { static_cast<unsigned char>(addr), data };
    dev->WriteToPipeIn(0x81, 2, buf);
}

unsigned short USBInputSource::readGSERegister(int addr) {
    unsigned char buf[2] = { static_cast<unsigned char>(addr), 0 };
    dev->WriteToPipeIn(0x81, 2, buf);
    dev->ReadFromPipeOut(0xA1, 2, buf);
    return static_cast<unsigned short>(buf[0] + 256 * buf[1]);
}
