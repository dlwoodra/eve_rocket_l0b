#include "USBInputSource.hpp"
#include "RecordFileWriter.hpp"
#include "FITSWriter.hpp"

#include <fstream>   // For file I/O
#include <cstdint>   // For uint32_t
#include <iostream>  // For error checking/logging

//#include <algorithm> // for lower_bound (searching for LUT_APID)

#define WORDS_TO_BYTES(words) ((words) << 2)    // Multiply words by 4
#define BYTES_TO_WORDS(bytes) ((bytes) >> 2)    // Divide bytes by 4

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

extern ProgramState globalState;
extern void handleSigint(int signal);

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
  MEGSA_APID, MEGSB_APID, ESP_APID, MEGSP_APID, SHK_APID 
  };
const uint16_t USBInputSource::LUT_PktLen[USBInputSource::nAPID] = { 
    STANDARD_MEGSAB_PACKET_LENGTH, // MA
    STANDARD_MEGSAB_PACKET_LENGTH, // MB
    STANDARD_MEGSAB_PACKET_LENGTH, // ESP
    STANDARD_MEGSAB_PACKET_LENGTH, // MP
    STANDARD_MEGSAB_PACKET_LENGTH }; // SHK

constexpr int FindAPIDIndex(uint16_t APID) {
    if (APID == 601) return 0;
    else if (APID == 602) return 1;
    else if (APID == 604) return 2;
    else if (APID == 605) return 3;
    else if (APID == 606) return 4;
    else return -1; // Not found
}

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
    size_t bytesRead = dev->ReadFromBlockPipeOut(0xA3, blockSize, transferLength, (unsigned char*)RxBuff);
    return bytesRead; // if negative, there was an error
}

extern void processPackets(CCSDSReader& pktReader, \
    std::unique_ptr<RecordFileWriter>& recordWriter, \
    bool skipRecord);

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

        //powerOnLED();
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

// unused function
bool USBInputSource::read(uint8_t* buffer, size_t maxSize) {
    std::cout << "USBSource::read calling ReadFromBlockPipeOut" << std::endl;
    size_t bytesRead = dev->ReadFromBlockPipeOut(0xA3, 1024, maxSize, reinterpret_cast<unsigned char*>(buffer));
    std::cout << "***  *** *** USBSource::read ReadFromBlockPipeOut returned " << bytesRead << " bytes" << std::endl;

    return bytesRead == maxSize;
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
    // valgrind says FrontPanel devices.Open has a bug
    // ==22657== Source and destination overlap in memcpy(0x6e25018, 0x6e25018, 284)
    // ==22657==    at 0x486CF7C: __GI_memcpy (in /usr/libexec/valgrind/vgpreload_memcheck-arm64-linux.so)
    // ==22657==    by 0x4D24907: okCvFrontPanel::UpdateDeviceInfo() (in /usr/local/lib/libokFrontPanel.so)

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
    std::cout << "Found a device: " << m_devInfo.productName << "\n"; //XEM7310-A75 
    std::cout << "deviceID    : " << m_devInfo.deviceID << "\n"; 
    std::cout << "serialNumber: " << m_devInfo.serialNumber << "\n"; 
    // std::cout << "productName : " << m_devInfo.productName << "\n"; 
    // std::cout << "productID   : " << m_devInfo.productID << "\n"; 
    // std::cout << "deviceMajorVersion deviceMinorVersion   : " << m_devInfo.deviceMajorVersion << " " << m_devInfo.deviceMinorVersion << "\n"; 
    // std::cout << "hostInterfaceMajorVersion hostInterfaceMinorVersion   : " << m_devInfo.hostInterfaceMajorVersion << " " << m_devInfo.hostInterfaceMinorVersion << "\n"; 
    // std::cout << "wireWidth   : " << m_devInfo.wireWidth << "\n"; 
    // std::cout << "triggerWidth: " << m_devInfo.triggerWidth << "\n"; 
    // std::cout << "pipeWidth   : " << m_devInfo.pipeWidth << "\n"; 
    // std::cout << "registerAddressWidth   : " << m_devInfo.registerAddressWidth << "\n"; 
    // std::cout << "registerDataWidth   : " << m_devInfo.registerDataWidth << "\n"; 
    // std::cout << "pipeWidth   : " << m_devInfo.pipeWidth << "\n"; 
    // // usbSpeed 0=unknown, 1=FULL, 2=HIGH, 3=SUPER
    // std::cout << "USBSpeed    : " << m_devInfo.usbSpeed << "\n";
    // // okDeviceInterface 0=Unknown, 1=USB2, 2=PCIE, 3=USB3
    // std::cout << "okDeviceInterface: " << m_devInfo.deviceInterface << "\n";
    // // fpgaVendor 0=unknown, 1=XILINX, 2=INTEL
    // std::cout << "fpgaVendor: " << m_devInfo.fpgaVendor << "\n";


    // Alan loads the GSE firmware from a file
    // this is the one David gave me
    //dev->ConfigureFPGA("firmware/hss_usb_fpga_2024_08_28.bit");

    // Load the default PLL config into EEPROM
    dev->LoadDefaultPLLConfiguration();

    resetInterface(10); 
    //powerOnLED();// turn on the light

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
    std::cout << std::hex; // set hex default for numbers
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
            //raise(SIGINT); // fatal
    }
    std::cout << std::dec; // revert to decimal numbers

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

int32_t USBInputSource::copyToPackedBuffer(uint32_t startWordIndex, uint32_t* strippedRxBuff) {

    // Attempt to properly handle the case where a packet is split across two 64k reads.
    // The concept is to process the 64k buffer until the amount of valid data remaining 
    // to be processed is the same as the largest packet length (in 32-bit words).
    // The FPGA reports the number of valid transferred 32-bit words in the first word of each 1024 byte block.
    // The first word only ranges from 0 to 255 because it is 32-bit words.

    // Copy only the valid data from RxBuff into strippedRxBuff from each 1024 byte (256 32-bit word) block,
    // stripping off the first 4 bytes from each 1024 byte block to fill the array similar to RxBuff.
    // Count the number of valid words in the stripped buffer, so we can tell when we get within one 
    // packet length of the end of the buffer.

    // startIndex is in words
    uint32_t totalTransferredBytes = WORDS_TO_BYTES(startWordIndex); // the previous read ended at startIndex
    // arrays are defined private in the USBInputSource.hpp
    // The RxBuff[16384] is 64*256 32-bit values or 16384 32-bit words or 65535 bytes
    // The strippedRxBuff[16827] is long enough to hold the max of 1020*64 bytes + the largest packet
    // from RxBuff if it was full and have unused 64*4 bytes
    for (int32_t blk = 0; blk<=63; ++blk) 
    {
        uint32_t validWordCountInBlock = blk << 8; // 0th 32-bit word in block is a counter of the words the FPGA marked valid in the block
        uint32_t startDataWordInBlock = validWordCountInBlock + 1; // skip the first 32-bit word
        int32_t validLengthInBytes = WORDS_TO_BYTES(RxBuff[validWordCountInBlock]); // first value in each 1024-byte block is the number of valid 32-bit words 
        // counted from the front
        //std::cout << "replaceCGProxRx blk: "<< blk <<" validLength: " << validLengthInBytes << " startIndex:" << startIndex <<std::endl;
        if (validLengthInBytes > 0) {
            totalTransferredBytes += validLengthInBytes;
            //std::cout<<"copyToPackedBuffer: validLengthInBytes: "<<validLengthInBytes<<" startWordIndex: "<<startWordIndex<<" startDataWordInBlock: "<<startDataWordInBlock<<std::endl;
            memcpy(&strippedRxBuff[startWordIndex], (&RxBuff[startDataWordInBlock]), validLengthInBytes); // skip first 32-bit word and copy remaining 1020 bytes at a time (255 32-bit words)
            startWordIndex += BYTES_TO_WORDS(validLengthInBytes); // 4 bytes per 32-bit word
        }
    }
    return totalTransferredBytes;
}

void logBufferContents(const uint32_t *pBlk) {
    std::ostringstream oss;
    for (uint32_t icnt=0; icnt<256; ++icnt)
    {
        oss << fmt::format("{:08x} ", byteswap_32(pBlk[icnt]));
    }
    LogFileWriter::getInstance().logInfo("logBufferContents 256 words");
    LogFileWriter::getInstance().logInfo(oss.str());
    oss.str(""); // Clear the stringstream
    oss.clear();
}

void USBInputSource::replaceCGProxRx(CCSDSReader& usbReader)
{
    std::cout << "Running replaceCGProcRx" << std::endl;
    static uint32_t numberOfRemainingUnusedBytes = 0;
    static uint32_t strippedRxBuff[lengthInWordsOfStrippedRxBuff]; //32-bit words

	static int16_t APID = 0;
    static uint32_t packetBuffer[maxPacketWords];

    // largest packet is 1772 bytes with the sync, so 443 words is enough
    //static uint32_t PktBuff[443]; // size to largest packet in 32-bit words
    //static uint32_t PktBuff[4096]; // size to largest packet in 32-bit words

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

    // for these params we should call this every ~65-75 milliseconds
    // for now just read the buffer iloop times as fast as possible
    // note 8/30/24 blockPipeOutStatus is 65536
    //uint32_t milliSecondWaitTimeBetweenReads = 2;
    //uint32_t numberOfCountersPerSecond = 1000 / milliSecondWaitTimeBetweenReads - 1;
    while (true) {
        //uint32_t waitCounter=0;

        // check for overflow
        checkLinkStatus();

        // read the data from the USB  - 64k bytes
        int32_t blockPipeOutStatus = readDataFromUSB();
        // returns number of bytes or <0 for errors
	    if ( blockPipeOutStatus < 0)
	    {
            std::cerr << "ERROR: USB Read Error" << std::endl;
            LogFileWriter::getInstance().logError("replaceCGProxRx blockPipeOutStatus Read error {}",blockPipeOutStatus);
		    return;
	    }
        if ( blockPipeOutStatus != 65536 ) {
            std::cout << "ERROR: replaceCGProxRx blockPipeOutStatus expected 65536 bytes read got "<<blockPipeOutStatus << std::endl;
            LogFileWriter::getInstance().logError("CGProxRx blockPipeOutStatus expected 65536 bytes read got {}", blockPipeOutStatus);
        }

        uint32_t startWordIndex = BYTES_TO_WORDS(numberOfRemainingUnusedBytes); // convert bytes to 32-bit words
        numberOfRemainingUnusedBytes = 0; // reset for this read

        // copy the new data to the buffer, after the previous remaining bytes from the last read
        // note the startIndex is passed by value, not reference, so it is not updated in the current scope
        uint32_t totalValidBytes = USBInputSource::copyToPackedBuffer(startWordIndex, strippedRxBuff);

        LogFileWriter::getInstance().logInfo("replaceCGProxRx: totalValidBytes:{}",totalValidBytes);

        // new approach 
        bool continueLookingForPackets = true;
        uint32_t byteIdx = 0;
        uint8_t *byteBuffer = reinterpret_cast<uint8_t*>(&strippedRxBuff[0]);
        while (continueLookingForPackets) {
            // look for sync marker
            while ( (byteIdx <= (totalValidBytes - maxPacketBytes)) &&
                (byteBuffer[byteIdx] != 0x1A) && (byteBuffer[byteIdx+1] != 0xCF) && 
                (byteBuffer[byteIdx+2] != 0xFC) && (byteBuffer[byteIdx+3] != 0x1D)) {
                // The FPGA respects 32-bit boundaries, so we can just increment by 4
                byteIdx += 4; // increment byte index into byteBuffer to the next word
            }
            if (byteIdx > (totalValidBytes - maxPacketBytes)) {
                // no sync marker found
                continueLookingForPackets = false;
            } else {
                // to get here, we found the sync marker
                // and we know all of the data in the buffer is valid
                // that means we have a complete packet in the buffer

                // print the sync marker to validate
                std::cout<< std::hex<<std::setw(2)<<std::setfill('0')<<"sync: ";
                for (int i=0; i<4; i++) {
                    std::cout<<static_cast<int>(byteBuffer[byteIdx+i]) << " ";
                }
                std::cout<<std::endl;

                // validate sync marker was found
                if ( (byteBuffer[byteIdx] != 0x1A) && (byteBuffer[byteIdx+1] != 0xCF) && 
                    (byteBuffer[byteIdx+2] != 0xFC) && (byteBuffer[byteIdx+3] != 0x1D)) 
                {
                    std::cout<< "ERROR: replaceCGProxRx sync marker not found at byteIdx:"<<byteIdx<<std::endl;
                    // print the next 20 bytes to see where we are
                    std::cout<< std::hex<<std::setw(2)<<std::setfill('0');
                    for (uint32_t i=byteIdx; i<byteIdx+20; i++) {
                        std::cout<<static_cast<int>(byteBuffer[i]);
                    }
                    std::cout<<std::endl;
                }

                byteIdx += 4; // skip past the sync marker
                //byteIdx is the start of the primary header

                std::cout<<std::endl;
                std::cout<< std::hex<<"sync with pkt: ";
                for (uint32_t i=byteIdx-4; i<std::min(byteIdx+16, totalValidBytes); i++) {
                    std::cout<<std::setw(2)<<std::setfill('0')<<static_cast<int>(byteBuffer[i])<<" ";
                }
                std::cout<<std::endl;

                APID = ((byteBuffer[byteIdx] << 8) & 0x0700) | byteBuffer[byteIdx+1];
                uint16_t pktLength = ((byteBuffer[byteIdx+4] << 8)) | ((byteBuffer[byteIdx+5]));
                std::cout<<std::hex<<"APID: "<<APID<<" pktLength: "<<pktLength<<std::endl;
                // copy to the packet buffer for processing
                uint32_t numBytesToCopy = pktLength + 1 + PACKET_HEADER_SIZE;
                std::cout<< std::dec<<"numBytestoCopy:"<<numBytesToCopy<<std::endl;

                for (uint32_t i=byteIdx-4; i<numBytesToCopy; i++) {
                    std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(byteBuffer[i]) << " ";
                }
                std::cout<<std::dec<<std::endl;

                std::cout<< "totalValidBytes:"<<totalValidBytes<<" numBTC:"<<numBytesToCopy<<" byteIdx:"<<byteIdx<<" APID: "<<APID<<" pktLen:"<<pktLength <<std::endl;

                std::cout<<"sync & full packet "<<std::hex;
                for (uint32_t i=byteIdx-4; i<numBytesToCopy; i++)
                {
                    std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(byteBuffer[i]) << " ";

                }
                std::cout<< std::dec<<std::endl;

                memcpy(&packetBuffer, &byteBuffer[byteIdx], numBytesToCopy);
                GSEProcessPacket(packetBuffer, APID, usbReader);
                byteIdx += numBytesToCopy; // skip past the packet
                if (byteIdx >= (totalValidBytes - maxPacketBytes)) {
                    continueLookingForPackets = false;
                }
            }

        } // the while continueLookingForPackets loop

        // we need to store the rest for the next read
        numberOfRemainingUnusedBytes = totalValidBytes - byteIdx;
        if (numberOfRemainingUnusedBytes > 0) 
        {
            std::cout<<"copying memory from byteBuffer to strippedRxBuff"<<std::endl;
            // copy the remaining bytes to the beginning of the buffer for next time
            memcpy(&strippedRxBuff[0], &byteBuffer[byteIdx], numberOfRemainingUnusedBytes);
            // zero out the rest of the buffer
            memset(&strippedRxBuff[BYTES_TO_WORDS(numberOfRemainingUnusedBytes)+1], 0, sizeof(strippedRxBuff)-(numberOfRemainingUnusedBytes-1)); // reset the buffer
        }

    } // the infinite while loop
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
	uint32_t* pBlk;

    static uint32_t APIDidx; // need to keep track of the APID index

    static uint16_t bytesCopiedToPktBuff = 0;

    //static uint32_t PktBuff[4096]; // size to largest packet in 32-bit words
    // this is overkill, largest packet is 1772 bytes with the sync, so 443 words is enough
    // total packet is 6+1761+1 bytes = 1768 bytes = 442 32-bit words
    static uint32_t PktBuff[442]; // size to largest packet in 32-bit words

    while (isReceiveFIFOEmpty()) {
        handleReceiveFIFOError();
        std::cout<<"Waiting for data..."<<std::endl;
    }

    // for these params we should call this every ~65-75 milliseconds
    // for now just read the buffer iloop times as fast as possible
    // note 8/30/24 blockPipeOutStatus is 65536

    while (true) {

        // check for overflow, reset linkif needed
        checkLinkStatus();

        int32_t blockPipeOutStatus = readDataFromUSB();

        globalState.totalReadCounter.fetch_add(1, std::memory_order_relaxed);
        
        // This is only used for testing. It generates a large file quickly.
        //writeBinaryToFile("./tmp.bin", RxBuff, sizeof(RxBuff) / sizeof(RxBuff[0]));

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
			    		//++ctrRxPkts;
			    	}
			    	++blkIdx; //move to the next 32-bit word
			    	--nBlkLeft; //decrement the number of 32-bit words left in the block
			    	break;

    			case 1:
    				// get APID index (MSB = 0, LSB = 1)
    				APID = (pBlk[blkIdx] << 8) & 0x0700; //APID is 11 bits;
    				APID |= ((pBlk[blkIdx] >> 8) & 0xFF);

    				// find APID index
                    i = FindAPIDIndex(APID);

    				// APID is recognized
    				if (i < nAPID)
    				{
                        //std::cout << "CGProxRx Case 1d recognized apid" << std::endl;
    					APIDidx = i;

    					nPktLeft = BYTES_TO_WORDS(LUT_PktLen[APIDidx]) + 3; // 11 bytes (fits into 3 32-bit words) for primary header and sync

    					// check to see if packet is completed in block
    					if (nPktLeft <= nBlkLeft)
    					{
    						// remaining packet is less then data remaining in block
    						nPktLeft &= 0xFF; // not sure what this does
                            //std::cout << "CGProxRx Case 1dd -copying - state "<<state << std::endl;
    						memcpy(PktBuff, &pBlk[blkIdx], WORDS_TO_BYTES(nPktLeft));
                            bytesCopiedToPktBuff += WORDS_TO_BYTES(nPktLeft); // count bytes copied to PktBuff
                            // bytesCopiedToPktBuff should match the total packet length (no sync)
                            LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 1 blk:{} nPktLeft:{} nBlkLeft:{} ",blk,nPktLeft,nBlkLeft);
    						nBlkLeft -= (nPktLeft);
    						blkIdx += (nPktLeft); // test

	    					state = 0; // this should make it start looking for the sync marker again

                            LogFileWriter::getInstance().logInfo("CGProxRx: case 1 complete Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                            uint32_t expectedNumBytes = LUT_PktLen[APIDidx] + 1 + PACKET_HEADER_SIZE;
                            if (bytesCopiedToPktBuff != (expectedNumBytes)) {
                                std::cerr << "***ERROR: CGProxRx case 1 bytesCopiedToPktBuff does not match expectedNumBytes" << std::endl;
                            }
    						GSEProcessPacket(PktBuff, APID, usbReader);
                            bytesCopiedToPktBuff = 0; // reset the count
		    			}
		    			else
		    			{
		    				// packet data is longer than data remaining in block
		    				nBlkLeft &= 0xFF;
                            //std::cout << "CGProxRx Case 1ddd -copying - state "<<state << std::endl;
                            LogFileWriter::getInstance().logInfo("CGProxRx: case 1, partial packet at end of 64k buffer, Blockdata dump, APID:{} blkIdx:{} nPktLeft:{} pktIdx:{} nBlkLeft:{}",APID,blkIdx,nPktLeft,pktIdx,nBlkLeft);
		    				memcpy(PktBuff, &pBlk[blkIdx], WORDS_TO_BYTES(nBlkLeft)); //nBlkL
                            bytesCopiedToPktBuff += WORDS_TO_BYTES(nBlkLeft); // count bytes copied to PktBuff
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
	    			}
	    			break;

	    		case 2:
                    //std::cout << "CGProxRx Case 1e -contination - state "<<state << std::endl;
    				// continuation of packet into new block
		    		pktIdx &= 0x7FF;
		    		if (nPktLeft <= nBlkLeft)
		    		{
		    			// remaining packet data is less than data left in block
		    			nPktLeft &= 0xFF;
                        //std::cout << "Case 1ee -contination copy - state "<<state << std::endl;
		    			memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], WORDS_TO_BYTES(nPktLeft));
                        bytesCopiedToPktBuff += WORDS_TO_BYTES(nPktLeft); // count bytes copied to PktBuff
                        LogFileWriter::getInstance().logInfo("CGProxRx: ProcessPacket case 2 nPktLeft:{} nBlkLeft:{} blkIdx:{}",nPktLeft,nBlkLeft,blkIdx);
		    			nBlkLeft -= nPktLeft;
		    			blkIdx += (nPktLeft);

    					state = 0;

                        LogFileWriter::getInstance().logInfo("CGProxRx: case 2 Blockdata dump, APID:{} blkIdx:{} nPktLeft:{}",APID,blkIdx-(nPktLeft-2),nPktLeft);

                        //logBufferContents(pBlk);
                        // logging the block data

                        //std::cout << "Case 1eee -call GSEProcessPacket - state " <<state<< std::endl;
                        uint32_t expectedNumBytes = LUT_PktLen[APIDidx] + 1 + PACKET_HEADER_SIZE;
                        if (bytesCopiedToPktBuff != (expectedNumBytes)) {
                        //    std::cerr << "***ERROR: CGProxRx case 2 bytesCopiedToPktBuff "<< bytesCopiedToPktBuff<<" does not match expectedNumBytes "<< expectedNumBytes << std::endl;
                            LogFileWriter::getInstance().logError("CGProxRx case 2 bytesCopiedToPktBuff {} does not match LUT_PktLen",bytesCopiedToPktBuff);
                        }
    					GSEProcessPacket(PktBuff, APID, usbReader);
                        bytesCopiedToPktBuff = 0; // reset the count
    				}
    				else
    				{
    					// packet data is longer than data remaining in block
    					nBlkLeft &= 0xFF;
                        //std::cout << "Case 1f - memcpy - state " <<state<< std::endl;
    					memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], WORDS_TO_BYTES(nBlkLeft));
                        bytesCopiedToPktBuff += WORDS_TO_BYTES(nBlkLeft); // count bytes copied to PktBuff
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
	uint16_t r0 = readGSERegister(0);
	uint16_t r1 = readGSERegister(1);
	uint16_t r2 = readGSERegister(2); 
	uint16_t r3 = readGSERegister(3); 

    // atmoic store the register values, no mutex needed
    globalState.FPGA_reg0.store(r0, std::memory_order_relaxed);
    globalState.FPGA_reg1.store(r1, std::memory_order_relaxed);
    globalState.FPGA_reg2.store(r2, std::memory_order_relaxed);
    globalState.FPGA_reg3.store(r3, std::memory_order_relaxed);

    // r0 bit 2, 3rd bit, is FIFO error bit - the same as register 2 bit 1
	while (r2 == 1)
	{
		std::cout << StatusStr << "checkLinkStatus: FIFO Overflow ************** "<< " " << r2 << std::endl;
        // reset the interface to clear the buffer
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
    
    static uint32_t totalPacketCounter=0;

    // increment the total packet counter
    totalPacketCounter++;

    if (APID == ESP_APID) {
        globalState.packetsPerSecond.store(totalPacketCounter, std::memory_order_relaxed);    
        globalState.readsPerSecond.store(globalState.totalReadCounter.load());
        globalState.totalReadCounter.store(0);
        totalPacketCounter = 0;
    }

    // extract the pktlength in the byte reversed 32-bit array
    // for MEGS-A, PktBuff[1] is e106, which should be 06e1
    uint16_t packetLength = ((PktBuff[1] >> 8) & 0xFF) | ((PktBuff[1] << 8) & 0xFF00);

    // Calculate the total number of bytes
    uint16_t packetTotalBytes = PACKET_HEADER_SIZE + 1 + packetLength; //size to copy, 6 byte hdr, CCSDS pktLen+1

    if (packetTotalBytes < STANDARD_MEGSP_PACKET_LENGTH) {
        std::cerr << "ERROR: GSEProcessPacket has short packet" << std::endl;
        LogFileWriter::getInstance().logError("GSEProcessPacket has short packet for APID {}", APID);
        globalState.shortPacketCounter.fetch_add(1);
        return;
    }

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



// Function to append 32-bit words to a binary file
void USBInputSource::writeBinaryToFile(const std::string& filename, const uint32_t* data, size_t size) {
    // Open the file in binary mode and append (std::ios::binary | std::ios::app)
    std::ofstream file(filename, std::ios::binary | std::ios::app);

    // Check if the file was opened successfully
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file: " << filename << std::endl;
        return;
    }

    // Write the 32-bit words to the file
    file.write(reinterpret_cast<const char*>(data), size * sizeof(uint32_t));

    // Check if writing succeeded
    if (!file) {
        std::cerr << "Error: Failed to write to file: " << filename << std::endl;
    }

    // Close the file
    file.close();
}
