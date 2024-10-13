#include "USBInputSource.hpp"
#include "RecordFileWriter.hpp"
#include "FITSWriter.hpp"
#include <algorithm> // for lower_bound (searching for LUT_APID)

struct DeviceInfo {
    std::string productName;
    uint32_t deviceID;
    std::string serialNumber;
    uint32_t productID;
    uint16_t deviceMajorVersion;
    uint16_t deviceMinorVersion;
    uint16_t hostInterfaceMajorVersion;
    uint16_t hostInterfaceMinorVersion;
    uint32_t wireWidth;
    uint32_t triggerWidth;
    uint32_t pipeWidth;
    uint32_t registerAddressWidth;
    uint32_t registerDataWidth;
    uint32_t usbSpeed;
    uint32_t deviceInterface;
    uint32_t fpgaVendor;
};

// Function to log device information using spdlog
void logDeviceInfo(const okTDeviceInfo& devInfo) {
    // Initialize the logger (log to file with basic file sink)
    auto logger = spdlog::basic_logger_mt("device_logger", "device_info.log");

    // Log all the device information
    logger->info("deviceID    : {}", devInfo.deviceID);
    logger->info("serialNumber: {}", devInfo.serialNumber);
    logger->info("productName : {}", devInfo.productName);
    logger->info("productID   : {}", devInfo.productID);
    logger->info("deviceMajorVersion deviceMinorVersion   : {} {}", devInfo.deviceMajorVersion, devInfo.deviceMinorVersion);
    logger->info("hostInterfaceMajorVersion hostInterfaceMinorVersion   : {} {}", devInfo.hostInterfaceMajorVersion, devInfo.hostInterfaceMinorVersion);
    logger->info("wireWidth   : {}", devInfo.wireWidth);
    logger->info("triggerWidth: {}", devInfo.triggerWidth);
    logger->info("pipeWidth   : {}", devInfo.pipeWidth);
    logger->info("registerAddressWidth   : {}", devInfo.registerAddressWidth);
    logger->info("registerDataWidth   : {}", devInfo.registerDataWidth);
    logger->info("pipeWidth   : {}", devInfo.pipeWidth);

    // Map usbSpeed value to meaningful string
    std::string usbSpeed;
    switch (devInfo.usbSpeed) {
        case 0: usbSpeed = "Unknown"; break;
        case 1: usbSpeed = "FULL"; break;
        case 2: usbSpeed = "HIGH"; break;
        case 3: usbSpeed = "SUPER"; break;
        default: usbSpeed = "Invalid";
    }
    logger->info("USBSpeed    : {}", usbSpeed);

    // Map deviceInterface value to meaningful string
    std::string deviceInterface;
    switch (devInfo.deviceInterface) {
        case 0: deviceInterface = "Unknown"; break;
        case 1: deviceInterface = "USB2"; break;
        case 2: deviceInterface = "PCIE"; break;
        case 3: deviceInterface = "USB3"; break;
        default: deviceInterface = "Invalid";
    }
    logger->info("okDeviceInterface: {}", deviceInterface);

    // Map fpgaVendor value to meaningful string
    std::string fpgaVendor;
    switch (devInfo.fpgaVendor) {
        case 0: fpgaVendor = "Unknown"; break;
        case 1: fpgaVendor = "XILINX"; break;
        case 2: fpgaVendor = "INTEL"; break;
        default: fpgaVendor = "Invalid";
    }
    logger->info("fpgaVendor: {}", fpgaVendor);

    // Ensure the log file is flushed
    logger->flush();
}

// APIDs MUST be sorted in increasing numerical order
const uint16_t USBInputSource::LUT_APID[USBInputSource::nAPID] = { 
  MEGSA_APID, MEGSB_APID, ESP_APID, MEGSP_APID, HK_APID 
  };
const uint16_t USBInputSource::LUT_PktLen[USBInputSource::nAPID] = { 
    STANDARD_MEGSAB_PACKET_LENGTH, 
    STANDARD_MEGSAB_PACKET_LENGTH, 
    STANDARD_ESP_PACKET_LENGTH, 
    STANDARD_MEGSP_PACKET_LENGTH, 
    STANDARD_HK_PACKET_LENGTH };

// replacement for Windows sleep
void Sleep(int32_t milliSeconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliSeconds));
}

void extern processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet);

// Generate a filename based on the current date and time
std::string generateUSBRecordFilename() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm buf;
    localtime_r(&in_time_t, &buf);

    std::ostringstream oss;
    oss << std::put_time(&buf, "recordUSB_%Y_%j_%H_%M_%S") << ".rtlm";

    return oss.str();
}

std::ofstream USBInputSource::initializeOutputFile() {
    std::string usbRecordFilename = generateUSBRecordFilename();
    std::ofstream outputFile(usbRecordFilename, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "ERROR: Failed to open USB record file for writing: " << usbRecordFilename << std::endl;
    }
    return outputFile;
}

bool USBInputSource::isReceiveFIFOEmpty() {
    uint8_t reg0 = readGSERegister(0);
    return ((reg0 >> 1) & 0x01) != 0;
}

void USBInputSource::handleReceiveFIFOError() {
    std::cerr << "ERROR: FPGA stat_rx_empty register - no data to read" << std::endl;
    Sleep(10);
}

int32_t USBInputSource::readDataFromUSB() {
    int16_t blockSize = 1024; // bytes
    int32_t transferLength = blockSize * 64; // bytes to read
    //return dev->ReadFromBlockPipeOut(0xA3, blockSize, transferLength, (unsigned char*)this->rxBuffer);
    //return dev->ReadFromBlockPipeOut(0xA3, blockSize, transferLength, (unsigned char*)this->RxBuff);
    return dev->ReadFromBlockPipeOut(0xA3, blockSize, transferLength, (unsigned char*)RxBuff);
}

// void USBInputSource::GSEprocessBlock(uint8_t* pBlk) {
//     // * pBlk is a pointer to the current block of data in the buffer. (&RxBuff[256*blk])
//     // This is where the function will read the data from

//     // blkIdx is the index into the block tracking where we are as we process the block
//     uint16_t blkIdx = 1; 
//     // nBlkLeft is a reference to the number of data words left to process in the current block so we don't read beyond the data.
//     uint16_t nBlkLeft = pBlk[0]; 


//     while (nBlkLeft > 0) {
//         switch (state) {   //(this->state) {
//         case 0:
//             //GSEprocessPacketHeader(pBlk, blkIdx, nBlkLeft, this->state, this->APID, this->pktIdx, this->nPktLeft);
//             GSEprocessPacketHeader(pBlk, blkIdx, nBlkLeft, state, APID, pktIdx, nPktLeft);
//             break;
//         case 1:
//             std::cout<<"state "<<state<<" nBlkLeft "<<nBlkLeft<<std::endl;
//             //GSEprocessPacketContinuation(pBlk, blkIdx, nBlkLeft, this->pktIdx, this->nPktLeft, this->state);
//             GSEprocessPacketContinuation(pBlk, blkIdx, nBlkLeft, pktIdx, nPktLeft, state);
//             break;
//         case 2:
//             std::cout<<"state "<<state<<" nBlkLeft "<<nBlkLeft<<std::endl;
//             //GSEContinuePacket(pBlk, blkIdx, nBlkLeft, this->state, this->pktIdx, this->nPktLeft, APIDidx);
//             GSEContinuePacket(pBlk, blkIdx, nBlkLeft, state, pktIdx, nPktLeft, APIDidx);
//             break;
//         default:
//             //this->state = 0;
//             state = 0;
//         }
//     }
// }

// void USBInputSource::GSEprocessPacketHeader(uint8_t *& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, int16_t& state, int16_t& APID, uint16_t& pktIdx, uint16_t& nPktLeft) {
//     // The whole 32-bit pointer array is 32-bit byte swapped
//     // When using data, use reinterpret_cast<char *> to access the bytes
//     // the bytes are in correct network (big endian) order
//     //if ( pBlk[blkIdx] != 0) {
//     //    std::cout<<"GSEprocessPacketHeader: pBlk[blkdIdx] "<<std::hex<< (pBlk[blkIdx]) <<std::endl;
//     //}
//     if (pBlk[blkIdx] == 0x1DFCCF1A) {
//         pktIdx = 0;
//         state = 1;
//         ++ctrRxPkts;
//         std::cout << "GSEprocessPacketHeader: Found sync marker" << std::endl;
//     }
//     ++blkIdx;
//     --nBlkLeft;
// }


// void USBInputSource::GSEprocessPacketContinuation(uint8_t*& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, uint16_t& pktIdx, uint16_t& nPktLeft, int16_t& state) {

//     // The 32-bit pointer is byte swapping the first 4 bytes
//     APID = ((pBlk[blkIdx] & 0x07) << 8) | ((pBlk[blkIdx] >> 8) & 0xFF); // byte swap and extract 11-bit APID

//     // find APIDidx into LUT_APID that matches APID
//     uint16_t APIDidx = std::lower_bound(LUT_APID, LUT_APID + nAPID, APID) - LUT_APID;

//     std::cout<<"GSEprocessPacketContinuation - APID: "<<APID<<" APIDidx "<<APIDidx<<std::endl;

//     if (APIDidx < nAPID && LUT_APID[APIDidx] == APID) {
//         nPktLeft = LUT_PktLen[APIDidx]; // the whole packet length
//         // check to see if packet is completed in block
//         if (nPktLeft <= nBlkLeft) {
//             // remaining packet content is less than data left in block
//             nPktLeft &= 0xFF;

//             // Alan's code and magic counters
//             memcpy(PktBuff, &pBlk[blkIdx], 4 * nPktLeft);
//             nBlkLeft -= nPktLeft;
//             blkIdx += nPktLeft;
//             state = 0;

//             // the sync marker is not included in pBlk, could decrement pBlk, but we need a vector anyway

//             // we can just read the packet length from the primary header
//             // total length is 6 byte pri hdr + pktlen field + 1
//             uint16_t pktLen = (7 + ((static_cast<uint16_t>(RxBuff[4]) << 8) | static_cast<uint16_t>(RxBuff[5])));

//             // define vector length to contain pktLen, the complete packet
//             std::vector<uint8_t> pktVec(pktLen);
//             // append the packet data into the vector
//             std::copy(RxBuff, RxBuff + pktLen, pktVec.begin());

//             std::cout<<"APID "<<APID<<" pktLen "<<pktLen<<std::endl;

//             std::cout<<"PktVec hdr: "<<std::hex<< static_cast<int>(pktVec[0])<< static_cast<int>(pktVec[1])<<static_cast<int>(pktVec[2])<<static_cast<int>(pktVec[3])<<static_cast<int>(pktVec[4])<<static_cast<int>(pktVec[5])<<std::dec<<std::endl;

//             // write to file
//             //std::cout << "writing " << sizeof(RxBuff) << " " << std::endl; // says 20000?
//             //outputFile.write(reinterpret_cast<const char* >(&RxBuff), sizeof(RxBuff));
//             //outputFile.write(reinterpret_cast<const char* >(pktVec.data()), sizeof(pktVec.data()));

//             // processPackets takes pktReader, recordWriter, skipRecord as args
//             // here the "packet" includes the 4 byte sync marker
//             if (!recordFileWriter->writeSyncAndPacketToRecordFile(pktVec)) {
//                 std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
//                 return;
//             }
//             //processOnePacket(pktReader, syncAndPktVec, 0);
//         } else {
//             // packet data is longer than data remining in block
//             nBlkLeft &= 0xFF;
//             memcpy(PktBuff, &pBlk[blkIdx], 4 * nBlkLeft);
//             pktIdx += nBlkLeft;
//             nPktLeft -= nBlkLeft;
//             nBlkLeft = 0;
//             state = 2;
//         }
//     } else {
//         // for an unknown APID, we should search for the next sync marker
//         state = 0;
//         snprintf(StatusStr, sizeof(StatusStr), "Unrecognized APID");
//     }
// }

// void USBInputSource::GSEContinuePacket(uint8_t*& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, int16_t& state, uint16_t& pktIdx, uint16_t& nPktLeft, int16_t APIDidx) {
//     // continuation of packet into new block
//     pktIdx &= 0x7FF;
//     if (nPktLeft <= nBlkLeft) {
//         // remaining data is less than the data left in the block
//         nPktLeft &= 0xFF;
//         memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nPktLeft);
//         nBlkLeft -= nPktLeft;
//         blkIdx += nPktLeft;

//         state = 0;

//         //ProcessPacket(APIDidx);
//         // same thing from case 1
//         uint16_t pktLen = (7 + ((static_cast<uint16_t>(RxBuff[4]) << 8) | static_cast<uint16_t>(RxBuff[5])));
//         // define vector length to contain pktLen, the complete packet
//         std::vector<uint8_t> pktVec(pktLen);
//         // append the packet data into the vector
//         std::copy(RxBuff, RxBuff + pktLen, pktVec.begin());
//         std::cout<<"APID "<<APID<<" pktLen "<<pktLen<<std::endl;
//         std::cout<<"PktVec hdr: "<<std::hex<<pktVec[0]<<pktVec[1]<<pktVec[2]<<pktVec[3]<<pktVec[4]<<pktVec[5]<<std::dec<<std::endl;
//         // processPackets takes pktReader, recordWriter, skipRecord as args
//         // here the "packet" includes the 4 byte sync marker
// //        if (!recordFileWriter->writeSyncAndPacketToRecordFile(pktVec)) {
// //            std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
// //            return;
// //        }

//     } else {
//         // packet data is longer than data remaining in block
//         nBlkLeft &= 0xFF;
//         memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nBlkLeft);
//         pktIdx += nBlkLeft;
//         nPktLeft -= nBlkLeft;
//         nBlkLeft = 0;
//     }
// }


extern void processPackets(CCSDSReader& pktReader, \
    std::unique_ptr<RecordFileWriter>& recordWriter, \
    bool skipRecord);
    //std::unique_ptr<FITSWriter>& fitsFileWriter

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
        dev(nullptr),
        recordFileWriter(std::unique_ptr<RecordFileWriter>(new RecordFileWriter())) {
        // c++14 allows this recordFileWriter(std::make_unique<RecordFileWriter>())

        open();
        if (isOpen()) {
            std::cout << "USBInputSource constructor: recordFileWriter is open" << std::endl;
        }  

        std::cout << "USBInputSource constructor initialized." << std::endl;

        powerOnLED();
      }


USBInputSource::~USBInputSource() {
    close();
    //setGSERegister(1, 0); //turn off LED
    powerOffLED();
    recordFileWriter->close();
}

bool USBInputSource::open() {
    std::cout << "Opening USBInputSource for serial number: " << serialNumber << std::endl;

    // the constructor creates the recordFileWriter
    if (!recordFileWriter) {
        std::cerr << "ERROR: USBInputSource::open Failed to open record file."<<std::endl;
        return false;
    }
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

    recordFileWriter->close();
}

bool USBInputSource::read(uint8_t* buffer, size_t maxSize) {
    std::cout << "USBSource::read calling ReadFromBlockPipeOut" << std::endl;
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
    std::string serialNumber = "24080019Q1"; //replace after probing the HW
    std::cout << "First board connected has serialNumber " << serialNumber << std::endl;

    okCFrontPanel dev;
    int devCount = dev.GetDeviceCount();

    std::cout << "selectUSBSerialNumber: Number of Opal Kelly devices found: " << devCount << std::endl;
    if ( devCount == 0 ) {
        std::cout << "ERROR: Fatal - no Opal Kelly devices found - cannot continue" << std::endl;
        std::cout << " *** CONTINUING FOR DEBUGGING PURPOSES ***" << std::endl;
    } else {
        for (int i=0; i < devCount; i++) {
	        std::cout << "selectUSBSerialNumber: Device[" << i << "] Model : " << dev.GetDeviceListModel(i) << "\n"; // 43
	        std::cout << "selectUSBSerialNumber: Device[" << i << "] Serial : " << dev.GetDeviceListSerial(i) << "\n"; // 24080019QI
        }
        std::cout << "selecting first USBSerial device " << std::endl;
        serialNumber = dev.GetDeviceListSerial(0); // choosing first one found
    }
    return serialNumber;
}

void USBInputSource::initializeGSE() {

    //std::cout<<"initializeGSE calling devices.Open"<<std::endl;
    LogFileWriter::getInstance().logInfo("initializeGSE calling devices.Open");

    // open device
    devptr = devices.Open(""); // get the first connected device
    dev = devptr.get();
    if (!dev) {
        int deviceCount = devices.GetCount();
        if (deviceCount > 1) {
            //std::cout << "ERROR: initializeGSE: Multiple devices found!" << std::endl;
            LogFileWriter::getInstance().logError("initializeGSE Multiple devices found!");
        }
        if (deviceCount == 0) { // if more than 1, GetCount returns the number
            //std::cout << "ERROR: No connected devices detected." << std::endl;
            LogFileWriter::getInstance().logError("initializeGSE No connected devices found!");
        } else {
            //std::cout << "ERROR: Device could not be opened." << std::endl;
            LogFileWriter::getInstance().logError("initializeGSE Device could not be opened!");
        }
        return;
    }

    dev->GetDeviceInfo(&m_devInfo);
    
    // Log the device information
    logDeviceInfo(m_devInfo);

    // std:endl also forces a flush, so could impact performance
    //LogFileWriter::getInstance().logInfo("initializeGSE productName " + (m_devInfo.productName));
    std::cout << "Found a device: " << m_devInfo.productName << "\n"; //XEM7310-A75 
    std::cout << "deviceID    : " << m_devInfo.deviceID << "\n"; 
    std::cout << "serialNumber: " << m_devInfo.serialNumber << "\n"; 
    std::cout << "productName : " << m_devInfo.productName << "\n"; 
    std::cout << "productID   : " << m_devInfo.productID << "\n"; 
    std::cout << "deviceMajorVersion deviceMinorVersion   : " << m_devInfo.deviceMajorVersion << " " << m_devInfo.deviceMinorVersion << "\n"; 
    std::cout << "hostInterfaceMajorVersion hostInterfaceMinorVersion   : " << m_devInfo.hostInterfaceMajorVersion << " " << m_devInfo.hostInterfaceMinorVersion << "\n"; 
    std::cout << "wireWidth   : " << m_devInfo.wireWidth << "\n"; 
    std::cout << "triggerWidth: " << m_devInfo.triggerWidth << "\n"; 
    std::cout << "pipeWidth   : " << m_devInfo.pipeWidth << "\n"; 
    std::cout << "registerAddressWidth   : " << m_devInfo.registerAddressWidth << "\n"; 
    std::cout << "registerDataWidth   : " << m_devInfo.registerDataWidth << "\n"; 
    std::cout << "pipeWidth   : " << m_devInfo.pipeWidth << "\n"; 
    // usbSpeed 0=unknown, 1=FULL, 2=HIGH, 3=SUPER
    std::cout << "USBSpeed    : " << m_devInfo.usbSpeed << "\n";
    // okDeviceInterface 0=Unknown, 1=USB2, 2=PCIE, 3=USB3
    std::cout << "okDeviceInterface: " << m_devInfo.deviceInterface << "\n";
    // fpgaVendor 0=unknown, 1=XILINX, 2=INTEL
    std::cout << "fpgaVendor: " << m_devInfo.fpgaVendor << "\n";


    // Alan loads the GSE firmware from a file
    // this is the one David gave me
    //dev->ConfigureFPGA("hss_usb_fpga_2024_08_28.bit");

    // Load the default PLL config into EEPROM
    dev->LoadDefaultPLLConfiguration();

    resetInterface(10); 
    powerOnLED();// turn on the light

    // get device temperature
	double DeviceTemp = (readGSERegister(3) >> 4) * 503.975 / 4096 - 273.15; //reports over 30 deg C at room temp
    std::cout << "FPGA Temperature: " << DeviceTemp << std::endl;

    // stat_rx_err & stat_rx_empty & stat_tx_empty(LSbit) (3-bits)
    for (int32_t iloop=0; iloop < 1 ; iloop++) {
        uint16_t reg0 = readGSERegister(0);
        Sleep(500);
        std::cout << "reg 0 " << std::hex << reg0 << std::dec << std::endl;
    }

    // firmware version
    uint16_t reg1 = readGSERegister(1);
    Sleep(100);
    // lsb is rxerror
    uint16_t reg2 = readGSERegister(2);
    Sleep(100);
    // FPGA temperature
    uint16_t reg3 = readGSERegister(3);
    std::cout << std::hex;
    std::cout << "reg 1 " << reg1 << std::endl;
    std::cout << "reg 2 " << reg2 << std::endl;
    std::cout << "reg 3 " << reg3 << std::endl;
    std::cout << std::dec;

    // register values changed in newer firmware
    uint16_t firmwareVer = readGSERegister(1); // probably HW specific
    gseType = firmwareVer >> 12; 
    // Alan sees gseType as 1, Sync-Serial
    std::cout << std::hex;
    switch (gseType) {
        case 1:
            std::cout << "Sync-Serial GSE. Firmware Version: " << firmwareVer << std::endl;
            break;
        case 2:
            std::cout << "SpaceWire GSE. Firmware Version: " << firmwareVer << std::endl;
            break;
        case 3:
            std::cout << "Parallel GSE. Firmware Version: " << firmwareVer << std::endl;
            break;
        default:
            std::cout << "Error. Unrecognized firmware: " << firmwareVer << std::endl;
            //return;
    }
    std::cout << std::dec;

    // Set status string
    snprintf(StatusStr, sizeof(StatusStr), "GOOD");

    std::cout << "GSE initialized." << std::endl;

}

void USBInputSource::resetInterface(int32_t milliSeconds) {

    // Reset Space interface
    setGSERegister(0, 1);
    setGSERegister(0, 0);

    LogFileWriter::getInstance().logWarning("USBInputSource::resetInterface called");

    //Sleep(milliSeconds);
}
void USBInputSource::powerOnLED() {
    std::cout << "powerLED: turning on LED" << "\n";
    // Turn on USB LED to show GSE is communicating
    setGSERegister(1, 1);
}
void USBInputSource::powerOffLED() {
    std::cout << "powerLED: turning off LED" << "\n";
    // Turn off USB LED (second one on the board)
    setGSERegister(1, 0);
}


void USBInputSource::ProcRx(CCSDSReader& usbReader) {
    CGProcRx(usbReader);
}

void USBInputSource::replaceCGProxRx(CCSDSReader& usbReader)
{

    std::cout << "Running replaceCGProcRx" << std::endl;
	static int16_t state = 0;
	static int16_t APID = 0;
	static uint32_t pktIdx = 0;
	static uint32_t nPktLeft = 0;

	uint16_t blkIdx = 0;
	uint16_t nBlkLeft, blk, i;
	uint32_t APIDidx;
    uint32_t* pBlk;

    while (isReceiveFIFOEmpty()) {
        handleReceiveFIFOError();
        std::cout<<"Waiting for data..."<<std::endl;
    }

    // LSB register bit is stat_tx_empty - don't need that bit
    //uint8_t reg0 = readGSERegister(0);
    //uint8_t stat_rx_empty = (reg0 >> 1 ) & 0x01;
    //uint8_t stat_rx_error = (reg0 >> 2) & 0x01; //overflow, we are not keeping up
    //if (stat_rx_error) {
    //    std::cout << "ERROR: FPGA stat_rx_error register - Overflow" << std::endl;
    //}

    //while ( stat_rx_empty ) {
    //    // the FPGA receive buffer is empty
    //    std::cout << "ERROR: FPGA stat_rx_empty register - no data to read" << std::endl;
    //    Sleep(10);
    //}

    // for these params we should call this every ~65-75 milliseconds
    // for now just read the buffer iloop times as fast as possible
    // note 8/30/24 blockPipeOutStatus is 65536
    //uint32_t milliSecondWaitTimeBetweenReads = 2;
    //uint32_t numberOfCountersPerSecond = 1000 / milliSecondWaitTimeBetweenReads - 1;
    while (true) {
        uint32_t waitCounter=0;

        // check for overflow
        checkLinkStatus();

        //while (isReceiveFIFOEmpty()) {
        //    waitCounter++;
        //    //if ((waitCounter % (numberOfCountersPerSecond)) == 0) {
        //        //resetInterface(milliSecondWaitTimeBetweenReads);
        //    //} 
        //    //handleReceiveFIFOError();
        //    //std::cout<<"ReceiveFIFOEmpty: waiting"<<std::endl;
        //    Sleep(1); // 2 millisec - need to tune
        //}
        if (waitCounter > 100) {
            std::cout<<"Warning: CGProcRx slow data transfer, waitCounter is high "<<waitCounter<<std::endl;
        //    LogFileWriter::getInstance().logWarning("CGProxRx slow data transfer, waitCounter is high {}", waitCounter);
            waitCounter = 0;
        }

        int32_t blockPipeOutStatus = readDataFromUSB();
        // returns number of bytes or <0 for errors
	    if ( blockPipeOutStatus < 0)
	    {
            std::cerr << "ERROR: USB Read Error" << std::endl;
            LogFileWriter::getInstance().logError("CGProxRx blockPipeOutStatus Read error {}",blockPipeOutStatus);
		    return;
	    }
        if ( blockPipeOutStatus != 65536 ) {
            std::cout << "ERROR: CGProxRx blockPipeOutStatus expected 65536 bytes read got "<<blockPipeOutStatus << std::endl;
            LogFileWriter::getInstance().logError("CGProxRx blockPipeOutStatus expected 65536 bytes read got {}", blockPipeOutStatus);
        }

        // copy the real data from RxBuff and strip off the first 4 bytes from each 1024 byte block to make a 64*1020/4 32-bit array similar to RxBuff
        for (blk = 0; blk<=63; ++blk) 
        {
            memcpy(&strippedRxBuff[blk*255], (&RxBuff[(blk*256) + 1]), 1020); // skip first 32-bit word and copy remaining 1020 bytes at a time (255 32-bit words)
        }

	    // // cycle through the whole buffer, no loop over blocks
	    //for (blk = 0; blk <= 63; ++blk)
	    //{
		    // get amount of data in block
		    pBlk = &strippedRxBuff[0];
		    nBlkLeft = pBlk[0];
		    blkIdx = 0;

		    while (nBlkLeft > 0)
		    {
			    switch (state)
			    {
			    case 0:
			    	// search for sync code - 0x1ACFFC1D
			    	if (pBlk[blkIdx] == 0x1DFCCF1A)
			    	{
			    		pktIdx = 0;
			    		state = 1;
			    		++ctrRxPkts;
			    	}
			    	++blkIdx; //move to the next 32-bit word
			    	--nBlkLeft; //decrement the number of 32-bit words left in the block
			    	break;

    			case 1:
    				// get APID index (MSB = 0, LSB = 1)
    				APID = (pBlk[blkIdx] << 8) & 0x0700; //Alan used 0x300, 10 bits, APID is 11 bits;
    				APID |= ((pBlk[blkIdx] >> 8) & 0xFF);

    				// find APID index
	    			for (i = 0; i < nAPID; ++i)
	    			{
	    				if (LUT_APID[i] == APID)
	    				{
                            //std::cout << "CGProxRx Case 1c - found apid" << APID <<" i="<<i<<std::endl;
	    					break;
	    				}
	    			}

    				// APID is recognized
    				if (i < nAPID)
    				{
                        //std::cout << "CGProxRx Case 1d recognized apid" << std::endl;
    					APIDidx = i;
    					nPktLeft = (LUT_PktLen[APIDidx] >> 2) + 3; // 11 bytes (fits into 3 32-bit words) for primary header and sync

    					// check to see if packet is completed in block
    					if (nPktLeft <= nBlkLeft)
    					{
    						// remaining packet is less then data in block
    						//nPktLeft &= 0xFF; // not sure what this does
                            //std::cout << "CGProxRx Case 1dd -copying - state "<<state << std::endl;
    						memcpy(PktBuff, &pBlk[blkIdx], nPktLeft << 2); //the bitshift is a div by 4 to convert bytes to 32-bit words
                            LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 1 nPktLeft:{} nBlkLeft:{} ",nPktLeft,nBlkLeft);
    						nBlkLeft -= (nPktLeft);
    						blkIdx += (nPktLeft-2); // -2 because we are already at the next word

	    					state = 0; // this should make it start looking for the sync marker again

                            LogFileWriter::getInstance().logInfo("CGProxRx: Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                            std::ostringstream oss;
                            int count = 0;
                            for (uint32_t icnt=0; icnt<255; ++icnt)
                            {
                                oss << fmt::format("{:08x} ", byteswap_32(pBlk[icnt])); //[blkIdx - nPktLeft + icnt]);
                                count++;

                                if (count == 254) // Long line
                                {
                                    LogFileWriter::getInstance().logInfo(oss.str());
                                    oss.str(""); // Clear the stringstream
                                    oss.clear();
                                    count = 0;
                                }
                            }

                            // Log any remaining values if less than 10 in the final line
                            if (count != 0)
                            {
                                LogFileWriter::getInstance().logInfo(oss.str());
                            }

                            //std::cout << "CGProxRx Case 1ddd -call GSEProcessPacket - state " <<state<< std::endl;
    						GSEProcessPacket(PktBuff, APID, usbReader);

	    					//state = 0; // this should make it start looking for the sync marker again
		    			}
		    			else
		    			{
		    				// packet data is longer than data remaining in block
		    				nBlkLeft &= 0xFF;
                            //std::cout << "CGProxRx Case 1ddd -copying - state "<<state << std::endl;
		    				memcpy(PktBuff, &pBlk[blkIdx], nBlkLeft << 2);
		    				pktIdx += nBlkLeft;
		    				nPktLeft -= nBlkLeft;
		    				nBlkLeft = 0;

			    			state = 2;
				    	}
				    }
	    			else
	    			{
	    				state = 0;
                        LogFileWriter::getInstance().logError("CGProxRx: Unrecognized APID {}",APID);
	    				//snprintf(StatusStr, sizeof(StatusStr),"CGProxRx Unrecognized APID             ");
	    			}
	    			break;

	    		case 2:
                    //std::cout << "CGProxRx Case 1e -contination - state "<<state << std::endl;
    				// continuation of packet into new blcok
		    		pktIdx &= 0x7FF;
		    		if (nPktLeft <= nBlkLeft)
		    		{
		    			// remaining packet data is less than data left in block
		    			nPktLeft &= 0xFF;
                        //std::cout << "Case 1ee -contination copy - state "<<state << std::endl;
		    			memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], nPktLeft << 2);
                        LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 2 nPktLeft:{} nBlkLeft:{} blkIdx:{}",nPktLeft,nBlkLeft,blkIdx);
		    			nBlkLeft -= nPktLeft;
		    			blkIdx += (nPktLeft-2);

    					state = 0;

                        LogFileWriter::getInstance().logInfo("CGProxRx: Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                        // logging the block data

                        std::ostringstream oss;
                        int count = 0;
                        // current block
                        for (uint32_t icnt=0; icnt<255*64; ++icnt)
                        {
                            oss << fmt::format("{:08x} ", byteswap_32(pBlk[icnt]));
                            count++;

                            if (count == 255*64 -1 ) // Roughly 80 characters (10 * 9 = 90 chars including spaces)
                            {
                                LogFileWriter::getInstance().logInfo(oss.str());
                                oss.str(""); // Clear the stringstream
                                oss.clear();
                                count = 0;
                            }
                        }
                        // Log any remaining values if less than 10 in the final line
                        if (count != 0)
                        {
                            LogFileWriter::getInstance().logInfo(oss.str());
                        }

                        //std::cout << "Case 1eee -call GSEProcessPacket - state " <<state<< std::endl;
    					GSEProcessPacket(PktBuff, APID, usbReader);
    				}
    				else
    				{
    					// packet data is longer than data remaining in block
    					nBlkLeft &= 0xFF;
                        //std::cout << "Case 1f - memcpy - state " <<state<< std::endl;
    					memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], nBlkLeft << 2);
    					pktIdx += nBlkLeft;
    					nPktLeft -= nBlkLeft;
    					nBlkLeft = 0;
    				}
    				break;

    			default:
    				state = 0;

    			}
    		}
        //} // iloop

	}
}

// *********************************************************************/
// Function: CGProcRx()
// Description: Process receive data
// *********************************************************************/
void USBInputSource::CGProcRx(CCSDSReader& usbReader)
{

    std::cout << "Running CGProcRx" << std::endl;
	static int16_t state = 0;
	static int16_t APID = 0;
	static uint32_t pktIdx = 0;
	static uint32_t nPktLeft = 0;

	uint16_t blkIdx = 0;
	uint16_t nBlkLeft, blk, i;
	uint32_t* pBlk, APIDidx;

    while (isReceiveFIFOEmpty()) {
        handleReceiveFIFOError();
        std::cout<<"Waiting for data..."<<std::endl;
    }

    // LSB register bit is stat_tx_empty - don't need that bit
    //uint8_t reg0 = readGSERegister(0);
    //uint8_t stat_rx_empty = (reg0 >> 1 ) & 0x01;
    //uint8_t stat_rx_error = (reg0 >> 2) & 0x01; //overflow, we are not keeping up
    //if (stat_rx_error) {
    //    std::cout << "ERROR: FPGA stat_rx_error register - Overflow" << std::endl;
    //}

    //while ( stat_rx_empty ) {
    //    // the FPGA receive buffer is empty
    //    std::cout << "ERROR: FPGA stat_rx_empty register - no data to read" << std::endl;
    //    Sleep(10);
    //}

    // for these params we should call this every ~65-75 milliseconds
    // for now just read the buffer iloop times as fast as possible
    // note 8/30/24 blockPipeOutStatus is 65536
    //uint32_t milliSecondWaitTimeBetweenReads = 2;
    //uint32_t numberOfCountersPerSecond = 1000 / milliSecondWaitTimeBetweenReads - 1;
    while (true) {
        uint32_t waitCounter=0;

        // check for overflow
        checkLinkStatus();

        //while (isReceiveFIFOEmpty()) {
        //    waitCounter++;
        //    //if ((waitCounter % (numberOfCountersPerSecond)) == 0) {
        //        //resetInterface(milliSecondWaitTimeBetweenReads);
        //    //} 
        //    //handleReceiveFIFOError();
        //    //std::cout<<"ReceiveFIFOEmpty: waiting"<<std::endl;
        //    Sleep(1); // 2 millisec - need to tune
        //}
        if (waitCounter > 100) {
            std::cout<<"Warning: CGProcRx slow data transfer, waitCounter is high "<<waitCounter<<std::endl;
        //    LogFileWriter::getInstance().logWarning("CGProxRx slow data transfer, waitCounter is high {}", waitCounter);
            waitCounter = 0;
        }

        int32_t blockPipeOutStatus = readDataFromUSB();
        // returns number of bytes or <0 for errors
	    if ( blockPipeOutStatus < 0)
	    {
            std::cerr << "ERROR: USB Read Error" << std::endl;
            LogFileWriter::getInstance().logError("CGProxRx blockPipeOutStatus Read error {}",blockPipeOutStatus);
		    return;
	    }
        if ( blockPipeOutStatus != 65536 ) {
            std::cout << "ERROR: CGProxRx blockPipeOutStatus expected 65536 bytes read got "<<blockPipeOutStatus << std::endl;
            LogFileWriter::getInstance().logError("CGProxRx blockPipeOutStatus expected 65536 bytes read got {}", blockPipeOutStatus);
        }


	    // cycle through 64 blocks
	    for (blk = 0; blk <= 63; ++blk)
	    {
		    // get amount of data in block
		    pBlk = &RxBuff[256 * blk];
		    nBlkLeft = pBlk[0];
		    blkIdx = 1;

		    while (nBlkLeft > 0)
		    {
			    switch (state)
			    {
			    case 0:
			    	// search for sync code - 0x1ACFFC1D
			    	if (pBlk[blkIdx] == 0x1DFCCF1A)
			    	{
			    		pktIdx = 0;
			    		state = 1;
			    		++ctrRxPkts;
			    	}
			    	++blkIdx; //move to the next 32-bit word
			    	--nBlkLeft; //decrement the number of 32-bit words left in the block
			    	break;

    			case 1:
    				// get APID index (MSB = 0, LSB = 1)
    				APID = (pBlk[blkIdx] << 8) & 0x0700; //Alan used 0x300, 10 bits, APID is 11 bits;
    				APID |= ((pBlk[blkIdx] >> 8) & 0xFF);

    				// find APID index
	    			for (i = 0; i < nAPID; ++i)
	    			{
	    				if (LUT_APID[i] == APID)
	    				{
                            //std::cout << "CGProxRx Case 1c - found apid" << APID <<" i="<<i<<std::endl;
	    					break;
	    				}
	    			}

    				// APID is recognized
    				if (i < nAPID)
    				{
                        //std::cout << "CGProxRx Case 1d recognized apid" << std::endl;
    					APIDidx = i;
    					nPktLeft = (LUT_PktLen[APIDidx] >> 2) + 3; // 11 bytes (fits into 3 32-bit words) for primary header and sync

    					// check to see if packet is completed in block
    					if (nPktLeft <= nBlkLeft)
    					{
    						// remaining packet is less then data in block
    						//nPktLeft &= 0xFF; // not sure what this does
                            //std::cout << "CGProxRx Case 1dd -copying - state "<<state << std::endl;
    						memcpy(PktBuff, &pBlk[blkIdx], nPktLeft << 2); //the bitshift is a div by 4 to convert bytes to 32-bit words
                            LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 1 blk:{} nPktLeft:{} nBlkLeft:{} ",blk,nPktLeft,nBlkLeft);
    						nBlkLeft -= (nPktLeft);
    						blkIdx += (nPktLeft-2); // -2 because we are already at the next word

	    					state = 0; // this should make it start looking for the sync marker again

                            LogFileWriter::getInstance().logInfo("CGProxRx: Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                            std::ostringstream oss;
                            int count = 0;
                            for (uint32_t icnt=0; icnt<256; ++icnt)
                            {
                                oss << fmt::format("{:08x} ", byteswap_32(pBlk[icnt])); //[blkIdx - nPktLeft + icnt]);
                                count++;

                                if (count == 255) // Long line
                                {
                                    LogFileWriter::getInstance().logInfo(oss.str());
                                    oss.str(""); // Clear the stringstream
                                    oss.clear();
                                    count = 0;
                                }
                            }

                            // Log any remaining values if less than 10 in the final line
                            if (count != 0)
                            {
                                LogFileWriter::getInstance().logInfo(oss.str());
                            }

                            //std::cout << "CGProxRx Case 1ddd -call GSEProcessPacket - state " <<state<< std::endl;
    						GSEProcessPacket(PktBuff, APID, usbReader);

	    					//state = 0; // this should make it start looking for the sync marker again
		    			}
		    			else
		    			{
		    				// packet data is longer than data remaining in block
		    				nBlkLeft &= 0xFF;
                            //std::cout << "CGProxRx Case 1ddd -copying - state "<<state << std::endl;
		    				memcpy(PktBuff, &pBlk[blkIdx], nBlkLeft << 2);
		    				pktIdx += nBlkLeft;
		    				nPktLeft -= nBlkLeft;
		    				nBlkLeft = 0;

			    			state = 2;
				    	}
				    }
	    			else
	    			{
	    				state = 0;
                        LogFileWriter::getInstance().logError("CGProxRx: Unrecognized APID {}",APID);
	    				//snprintf(StatusStr, sizeof(StatusStr),"CGProxRx Unrecognized APID             ");
	    			}
	    			break;

	    		case 2:
                    //std::cout << "CGProxRx Case 1e -contination - state "<<state << std::endl;
    				// continuation of packet into new blcok
		    		pktIdx &= 0x7FF;
		    		if (nPktLeft <= nBlkLeft)
		    		{
		    			// remaining packet data is less than data left in block
		    			nPktLeft &= 0xFF;
                        //std::cout << "Case 1ee -contination copy - state "<<state << std::endl;
		    			memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], nPktLeft << 2);
                        LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 2 nPktLeft:{} nBlkLeft:{} blkIdx:{}",nPktLeft,nBlkLeft,blkIdx);
		    			nBlkLeft -= nPktLeft;
		    			blkIdx += (nPktLeft-2);

    					state = 0;

                        LogFileWriter::getInstance().logInfo("CGProxRx: Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                        // logging the block data

                        std::ostringstream oss;
                        int count = 0;
                        // previous block is &RxBuff[256 * (blk-1)]
                        for (uint32_t icnt=0; icnt<256; ++icnt)
                        {
                            oss << fmt::format("{:08x} ", byteswap_32(*reinterpret_cast<uint32_t*>(&RxBuff[256*(blk-1) + icnt])));
                            count++;

                            if (count == 255) // Roughly 80 characters (10 * 9 = 90 chars including spaces)
                            {
                                LogFileWriter::getInstance().logInfo(oss.str());
                                oss.str(""); // Clear the stringstream
                                oss.clear();
                                count = 0;
                            }
                        }
                        count=0;
                        // current block
                        for (uint32_t icnt=0; icnt<256; ++icnt)
                        {
                            oss << fmt::format("{:08x} ", byteswap_32(pBlk[icnt]));
                            count++;

                            if (count == 255) // Roughly 80 characters (10 * 9 = 90 chars including spaces)
                            {
                                LogFileWriter::getInstance().logInfo(oss.str());
                                oss.str(""); // Clear the stringstream
                                oss.clear();
                                count = 0;
                            }
                        }
                        // Log any remaining values if less than 10 in the final line
                        if (count != 0)
                        {
                            LogFileWriter::getInstance().logInfo(oss.str());
                        }

                        //std::cout << "Case 1eee -call GSEProcessPacket - state " <<state<< std::endl;
    					GSEProcessPacket(PktBuff, APID, usbReader);
    				}
    				else
    				{
    					// packet data is longer than data remaining in block
    					nBlkLeft &= 0xFF;
                        //std::cout << "Case 1f - memcpy - state " <<state<< std::endl;
    					memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], nBlkLeft << 2);
    					pktIdx += nBlkLeft;
    					nPktLeft -= nBlkLeft;
    					nBlkLeft = 0;
    				}
    				break;

    			default:
    				state = 0;

    			}
    		}
        } // iloop

	}
}

// // unused
// void USBInputSource::processReceive() {
//     unsigned char* pBlkStart = rxBuffer;

//     // Check if receive FIFO is empty
//     if (readGSERegister(0) & 0x02) return;

//     // First read a whole block of data from the FPGA
//     // The buffer may be full, partly full, or empty
//     // and it contains the frame marker at the start of each block.
//     // The code skips that block and joins packets that span blocks.
//     // After that, search for the sync marker
//     // then get the 6-byte packet header
//     // then get the rest of the packet
    

//     // Get data, 64k at a time
//     if (dev->ReadFromBlockPipeOut(0xA3, 1024, 65536, rxBuffer) != 65536) {
//         std::cout << "\nAll data not returned" << std::endl;
//         return;
//     }

//     for (int i = 0; i <= 63; ++i) {
//         long dataLength = 4 * (*(reinterpret_cast<long*>(pBlkStart)));
//         if (dataLength > 1020) {
//             std::cout << "\nInvalid amount of data" << std::endl;
//             return;
//         }

//         if (dataLength) {
//             if (!telemetryOpen) {

//                 // This appears to be writing telemetry to a file, but not doing anything with it.
//                 pFileTelemetry = fopen("CSIE_Telemetry.bin", "wb");
//                 if (!pFileTelemetry) {
//                     std::cout << "Could not open Telemetry file" << std::endl;
//                     return;
//                 }
//                 telemetryOpen = true;
//                 ctrRxBytes = 0;
//                 openRxTime.updateNow();
//             }

//             pBlkStart += 4;
//             fwrite(pBlkStart, 1, dataLength, pFileTelemetry);
//             ctrRxBytes += dataLength;
//             pBlkStart += 1020;

//             lastRxTime.updateNow();
//         } else {
//             pBlkStart += 1024;
//         }
//     }
// }


// *********************************************************************/
// Function: CheckLinkStatus()
// Description: Check link status
// *********************************************************************/
void USBInputSource::checkLinkStatus(void)
{
	//uint16_t r;

	// get GSE status
    // bit definitions
    // r0 : 0 = tx FIFO empty
    // r0 : 1 = rx FIFO empty (we do not use this feature)
    // r0 : 2 = rx error
	//uint16_t r0 = readGSERegister(0);
	uint16_t r2 = readGSERegister(2); 
    // we think r0 bit 2, 3rd bit, is FIFO error bit
	while (r2 == 1)
	{
		std::cout << StatusStr << "checkLinkStatus: FIFO Overflow ************** "<< " " << r2 << std::endl;
        // to resolve, read until overflow is cleared
        //int32_t blockPipeOutStatus = readDataFromUSB();
        //std::cout << "checkLinkStatus blockPipeOutStatus: " << blockPipeOutStatus << std::endl;
        // read status again
        resetInterface(1);  
    	//r0 = readGSERegister(0);
        r2 = readGSERegister(2);
        LogFileWriter::getInstance().logError("checkLinkStatus: FIFO Overflow - not keeping up");
	}

}

// *********************************************************************/
// Function: SetGSERegister()
// Description: Sets GSE register
// *********************************************************************/
void USBInputSource::setGSERegister(int16_t addr, unsigned char data)
{
	uint32_t w;

	w = 0x8000 | addr;
	w <<= 16;
	w |= data;

	dev->SetWireInValue(0x00, 0);
	dev->UpdateWireIns();
	dev->SetWireInValue(0x00, w);
	dev->UpdateWireIns();
}

// *********************************************************************/
// Function: ReadGSERegister()
// Description: Sets GSE register
// *********************************************************************/
unsigned short USBInputSource::readGSERegister(int addr)
{
	uint32_t w;
	uint16_t stat;

	w = 0x800F0000 | addr;

	// set read address
	dev->SetWireInValue(0x00, 0);
	dev->UpdateWireIns();
	dev->SetWireInValue(0x00, w);
	dev->UpdateWireIns();

	// read data
	dev->UpdateWireOuts();
	stat = (uint16_t) dev->GetWireOutValue(0x20) & 0xFFFF;

	return stat;
}

void USBInputSource::GSEProcessPacket(uint32_t *PktBuff, uint16_t APID, CCSDSReader& usbReader) {
    
    // extract the pktlength in the byte reversed 32-bit array
    // for MEGS-A, PktBuff[1] is e106, which should be 06e1
    uint16_t packetLength = ((PktBuff[1] >> 8) & 0xFF) | ((PktBuff[1] << 8) & 0xFF00);

    // Calculate the total number of bytes
    uint16_t packetTotalBytes = 7 + packetLength; //size to copy, 6 byte hdr, CCSDS pktLen+1

    std::vector<uint8_t> packetVector;

    // Resize the vector to hold the bytes
    packetVector.resize(packetTotalBytes);

    // Copy the data from the uint32_t array to the uint8_t vector
    std::memcpy(packetVector.data(), reinterpret_cast<uint8_t*>(PktBuff), packetTotalBytes);

    // write the packet to the record file
    if (!recordFileWriter->writeSyncAndPacketToRecordFile(packetVector)) {
        std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
        return;
    }

    // send packetVector for packet processing
    processOnePacket(usbReader, packetVector);

    return;
}

