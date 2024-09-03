#include "USBInputSource.hpp"
#include "RecordFileWriter.hpp"
#include "FITSWriter.hpp"
#include <algorithm> // for lower_bound (searching for LUT_APID)

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

void GSEProcessPacket(uint16_t APID);

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
    return dev->ReadFromBlockPipeOut(0xA3, blockSize, transferLength, (unsigned char*)RxBuff);
}

void USBInputSource::GSEprocessBlock(uint32_t* pBlk) {
    // * pBlk is a pointer to the current block of data in the buffer. (&RxBuff[256*blk])
    // This is where the function will read the data from

    // blkIdx is the index into the block tracking where we are as we process the block
    uint16_t blkIdx = 1; 
    // nBlkLeft is a reference to the number of data words left to process in the current block so we don't read beyond the data.
    uint16_t nBlkLeft = pBlk[0]; 


    while (nBlkLeft > 0) {
        switch (this->state) {
        case 0:
            GSEprocessPacketHeader(pBlk, blkIdx, nBlkLeft, this->state, this->APID, this->pktIdx, this->nPktLeft);
            break;
        case 1:
            std::cout<<"state "<<state<<" nBlkLeft "<<nBlkLeft<<std::endl;
            GSEprocessPacketContinuation(pBlk, blkIdx, nBlkLeft, this->pktIdx, this->nPktLeft, this->state);
            break;
        case 2:
            std::cout<<"state "<<state<<" nBlkLeft "<<nBlkLeft<<std::endl;
            GSEContinuePacket(pBlk, blkIdx, nBlkLeft, this->state, this->pktIdx, this->nPktLeft, APIDidx);
            break;
        default:
            this->state = 0;
        }
    }
}

void USBInputSource::GSEprocessPacketHeader(uint32_t *& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, int16_t& state, int16_t& APID, uint16_t& pktIdx, uint16_t& nPktLeft) {
    // The whole 32-bit pointer array is 32-bit byte swapped
    // When using data, use reinterpret_cast<char *> to access the bytes
    // the bytes are in correct network (big endian) order
    //if ( pBlk[blkIdx] != 0) {
    //    std::cout<<"GSEprocessPacketHeader: pBlk[blkdIdx] "<<std::hex<< (pBlk[blkIdx]) <<std::endl;
    //}
    if (pBlk[blkIdx] == 0x1DFCCF1A) {
        pktIdx = 0;
        state = 1;
        ++ctrRxPkts;
        std::cout << "GSEprocessPacketHeader: Found sync marker" << std::endl;
    }
    ++blkIdx;
    --nBlkLeft;
}


void USBInputSource::GSEprocessPacketContinuation(uint32_t*& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, uint16_t& pktIdx, uint16_t& nPktLeft, int16_t& state) {

    // The 32-bit pointer is byte swapping the first 4 bytes
    APID = ((pBlk[blkIdx] & 0x07) << 8) | ((pBlk[blkIdx] >> 8) & 0xFF); // byte swap and extract 11-bit APID

    // find APIDidx into LUT_APID that matches APID
    uint16_t APIDidx = std::lower_bound(LUT_APID, LUT_APID + nAPID, APID) - LUT_APID;

    std::cout<<"GSEprocessPacketContinuation - APID: "<<APID<<" APIDidx "<<APIDidx<<std::endl;

    if (APIDidx < nAPID && LUT_APID[APIDidx] == APID) {
        nPktLeft = LUT_PktLen[APIDidx]; // the whole packet length
        // check to see if packet is completed in block
        if (nPktLeft <= nBlkLeft) {
            // remaining packet content is less than data left in block
            nPktLeft &= 0xFF;

            // Alan's code and magic counters
            memcpy(PktBuff, &pBlk[blkIdx], 4 * nPktLeft);
            nBlkLeft -= nPktLeft;
            blkIdx += nPktLeft;
            state = 0;

            // the sync marker is not included in pBlk, could decrement pBlk, but we need a vector anyway

            // we can just read the packet length from the primary header
            // total length is 6 byte pri hdr + pktlen field + 1
            uint16_t pktLen = (7 + ((static_cast<uint16_t>(RxBuff[4]) << 8) | static_cast<uint16_t>(RxBuff[5])));

            // define vector length to contain pktLen, the complete packet
            std::vector<uint8_t> pktVec(pktLen);
            // append the packet data into the vector
            std::copy(RxBuff, RxBuff + pktLen, pktVec.begin());

            std::cout<<"APID "<<APID<<" pktLen "<<pktLen<<std::endl;

            std::cout<<"PktVec hdr: "<<std::hex<< static_cast<int>(pktVec[0])<< static_cast<int>(pktVec[1])<<static_cast<int>(pktVec[2])<<static_cast<int>(pktVec[3])<<static_cast<int>(pktVec[4])<<static_cast<int>(pktVec[5])<<std::endl;

            // write to file
            //std::cout << "writing " << sizeof(RxBuff) << " " << std::endl; // says 20000?
            //outputFile.write(reinterpret_cast<const char* >(&RxBuff), sizeof(RxBuff));
            //outputFile.write(reinterpret_cast<const char* >(pktVec.data()), sizeof(pktVec.data()));

            // processPackets takes pktReader, recordWriter, skipRecord as args
            // here the "packet" includes the 4 byte sync marker
            if (!recordFileWriter->writeSyncAndPacketToRecordFile(pktVec)) {
                std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
                return;
            }
            //processOnePacket(pktReader, syncAndPktVec, 0);
        } else {
            // packet data is longer than data remining in block
            nBlkLeft &= 0xFF;
            memcpy(PktBuff, &pBlk[blkIdx], 4 * nBlkLeft);
            pktIdx += nBlkLeft;
            nPktLeft -= nBlkLeft;
            nBlkLeft = 0;
            state = 2;
        }
    } else {
        // for an unknown APID, we should search for the next sync marker
        state = 0;
        snprintf(StatusStr, sizeof(StatusStr), "Unrecognized APID");
    }
}

void USBInputSource::GSEContinuePacket(uint32_t*& pBlk, uint16_t& blkIdx, uint16_t& nBlkLeft, int16_t& state, uint16_t& pktIdx, uint16_t& nPktLeft, int16_t APIDidx) {
    // continuation of packet into new block
    pktIdx &= 0x7FF;
    if (nPktLeft <= nBlkLeft) {
        // remaining data is less than the data left in the block
        nPktLeft &= 0xFF;
        memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nPktLeft);
        nBlkLeft -= nPktLeft;
        blkIdx += nPktLeft;

        state = 0;

        //ProcessPacket(APIDidx);
        // same thing from case 1
        uint16_t pktLen = (7 + ((static_cast<uint16_t>(RxBuff[4]) << 8) | static_cast<uint16_t>(RxBuff[5])));
        // define vector length to contain pktLen, the complete packet
        std::vector<uint8_t> pktVec(pktLen);
        // append the packet data into the vector
        std::copy(RxBuff, RxBuff + pktLen, pktVec.begin());
        std::cout<<"APID "<<APID<<" pktLen "<<pktLen<<std::endl;
        std::cout<<"PktVec hdr: "<<std::hex<<pktVec[0]<<pktVec[1]<<pktVec[2]<<pktVec[3]<<pktVec[4]<<pktVec[5]<<std::endl;
        // processPackets takes pktReader, recordWriter, skipRecord as args
        // here the "packet" includes the 4 byte sync marker
//        if (!recordFileWriter->writeSyncAndPacketToRecordFile(pktVec)) {
//            std::cerr << "ERROR: processPackets failed to write packet to record file." << std::endl;
//            return;
//        }

    } else {
        // packet data is longer than data remaining in block
        nBlkLeft &= 0xFF;
        memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nBlkLeft);
        pktIdx += nBlkLeft;
        nPktLeft -= nBlkLeft;
        nBlkLeft = 0;
    }
}


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
            std::cout << "readNextPacket is open" << std::endl;
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
    // okDevideInterface 0=Unknown, 1=USB2, 2=PCIE, 3=USB3
    std::cout << "okDeviceInterface: " << m_devInfo.deviceInterface << "\n";
    // fpgaVendor 0=unknown, 1=XILINX, 2=INTEL
    std::cout << "fpgaVendor: " << m_devInfo.fpgaVendor << "\n";


    // Alan loads the GSE firmware from a file
    // this is the one David gave me
    //dev->ConfigureFPGA("hss_usb_fpga_2024_08_28.bit");

    // Load the default PLL config into EEPROM
    dev->LoadDefaultPLLConfiguration();

    resetInterface(100); 
    powerOnLED();// turn on the light

    // get device temperature
	double DeviceTemp = (readGSERegister(3) >> 4) * 503.975 / 4096 - 273.15; //reports over 30 deg C at room temp
    std::cout << "FPGA Temperature: " << DeviceTemp << std::endl;

    // stat_rx_err & stat_rx_empty & stat_tx_empty(LSbit) (3-bits)
    for (int32_t iloop=0; iloop < 1 ; iloop++) {
        uint16_t reg0 = readGSERegister(0);
        Sleep(500);
        std::cout << "reg 0 " << std::hex << reg0 << std::endl;
    }

    // firmware version
    uint16_t reg1 = readGSERegister(1);
    Sleep(100);
    // lsb is rxerror
    uint16_t reg2 = readGSERegister(2);
    Sleep(100);
    // FPGA temperature
    uint16_t reg3 = readGSERegister(3);
    std::cout << "reg 1 " << std::hex << reg1 << std::endl;
    std::cout << "reg 2 " << std::hex << reg2 << std::endl;
    std::cout << "reg 3 " << std::hex << reg3 << std::endl;

    // register values changed in newer firmware
    uint16_t firmwareVer = readGSERegister(1); // probably HW specific
    gseType = firmwareVer >> 12; // can this even work?
    // Alan sees gseType as 1, Sync-Serial
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
            //return;
    }

    // Set status string
    snprintf(StatusStr, sizeof(StatusStr), "GOOD");

    std::cout << "GSE initialized." << std::endl;

}

void USBInputSource::resetInterface(int32_t milliSeconds) {

    std::cout << "resetInterface turning on LED" << std::endl;
    // Reset Space interface
    setGSERegister(0, 1);
    setGSERegister(0, 0);
    Sleep(milliSeconds);

    // Turn on USB LED to show GSE is communicating
    setGSERegister(1, 1);
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


void USBInputSource::ProcRx() {
    CGProcRx();
}

// *********************************************************************/
// Function: CGProcRx()
// Description: Process receive data
// *********************************************************************/
void USBInputSource::CGProcRx(void)
{

    std::cout << "Running CGProcRx" << std::endl;
	//static int state = 0;
	//static int APID = 0;
	//static unsigned int pktIdx = 0;
	//static unsigned int nPktLeft = 0;

	//unsigned int blkIdx = 0;
	//unsigned int nBlkLeft, blk, i;
	//unsigned long* pBlk, APIDidx;

    while (isReceiveFIFOEmpty()) {
        handleReceiveFIFOError();
        std::cout<<"Waiting for data..."<<std::endl;
        //Sleep(250); //millisec
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
    // note 8/30/24 blockPipeOutStatus is 10000 (if hex then 65536)
    for ( int32_t iloop=0; iloop < 200; ++iloop) {
        //std::cout<< "Starting loop" <<std::endl;
        uint32_t waitCounter=0;
        while (isReceiveFIFOEmpty()) {
            waitCounter++;
            if ((waitCounter % 100) == 0) {
                resetInterface(2);
            } 
            //handleReceiveFIFOError();
            //std::cout<<"ReceiveFIFOEmpty: waiting"<<std::endl;
            Sleep(5); // 2 millisec - need to tune
        }
        std::cout<<"Wait counter "<<waitCounter<<std::endl;
        int32_t blockPipeOutStatus = readDataFromUSB();
        // returns number of bytes or <0 for errors
	    if ( blockPipeOutStatus < 0)
	    {
            std::cerr << "ERROR: USB Read Error" << std::endl;
		    return;
	    }
        std::cout << "blockPipeOutStatus bytes read "<<blockPipeOutStatus << std::endl; // says 10000, it is encoded as hex

        // process the blocks of data
        for (unsigned int blk = 0; blk <= 64; ++blk) {
            GSEprocessBlock(&RxBuff[256 * blk]); // 1024 bytes at a time
        }
    }

	// // cycle through 64 blocks
	// for (blk = 0; blk <= 63; ++blk)
	// {
	// 	//if (blk == 40)
	// 	//{
	// 	//	blk = blk;
	// 	//}

	// 	// get amount of data in block
	// 	pBlk = &RxBuff[256 * blk];
	// 	nBlkLeft = pBlk[0];
	// 	blkIdx = 1;

	// 	while (nBlkLeft > 0)
	// 	{
	// 		switch (state)
	// 		{
	// 		case 0:
	// 			// search for sync code - 0x1ACFFC1D
	// 			if (pBlk[blkIdx] == 0x1DFCCF1A)
	// 			{
	// 				pktIdx = 0;
	// 				state = 1;
	// 				++ctrRxPkts;
    //                 std::cout << "Found sync marker" << std::endl;
	// 			}
	// 			++blkIdx;
	// 			--nBlkLeft;
	// 			break;

	// 		case 1:
	// 			// get APID index (MSB = 0, LSB = 1)
	// 			APID = (pBlk[blkIdx] << 8) & 0x300;
	// 			APID |= ((pBlk[blkIdx] >> 8) & 0xFF);

	// 			// find APID index
	// 			for (i = 0; i < nAPID; ++i)
	// 			{
	// 				if (LUT_APID[i] == APID)
	// 				{
	// 					break;
	// 				}
	// 			}

	// 			// APID is recognized
	// 			if (i < nAPID)
	// 			{
	// 				APIDidx = i;
	// 				nPktLeft = LUT_PktLen[APIDidx];

	// 				// check to see if packet is completed in block
	// 				if (nPktLeft <= nBlkLeft)
	// 				{
	// 					// remaining packet is less then data in block
	// 					nPktLeft &= 0xFF;
	// 					memcpy(PktBuff, &pBlk[blkIdx], 4 * nPktLeft);
	// 					nBlkLeft -= nPktLeft;
	// 					blkIdx += nPktLeft;

	// 					state = 0;
	// 					GSEProcessPacket(APID);
	// 				}
	// 				else
	// 				{
	// 					// packet data is longer than data remaining in block
	// 					nBlkLeft &= 0xFF;
	// 					memcpy(PktBuff, &pBlk[blkIdx], 4 * nBlkLeft);
	// 					pktIdx += nBlkLeft;
	// 					nPktLeft -= nBlkLeft;
	// 					nBlkLeft = 0;

	// 					state = 2;
	// 				}
	// 			}
	// 			else
	// 			{
	// 				state = 0;
	// 				snprintf(StatusStr, sizeof(StatusStr),"Unrecognized APID             ");
	// 			}
	// 			break;

	// 		case 2:
	// 			// continuation of packet into new blcok
	// 			pktIdx &= 0x7FF;
	// 			if (nPktLeft <= nBlkLeft)
	// 			{
	// 				// remaing packet data is less than data left in block
	// 				nPktLeft &= 0xFF;
	// 				memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nPktLeft);
	// 				nBlkLeft -= nPktLeft;
	// 				blkIdx += nPktLeft;

	// 				state = 0;
	// 				GSEProcessPacket(APID);
	// 			}
	// 			else
	// 			{
	// 				// packet data is longer than data remaining in block
	// 				nBlkLeft &= 0xFF;
	// 				memcpy(&PktBuff[pktIdx], &pBlk[blkIdx], 4 * nBlkLeft);
	// 				pktIdx += nBlkLeft;
	// 				nPktLeft -= nBlkLeft;
	// 				nBlkLeft = 0;
	// 			}
	// 			break;

	// 		default:
	// 			state = 0;

	// 		}
	// 	}

	// }

    // done reading
    //outputFile.close();
    //if (outputFile.fail()) {
    //    std::cerr << "ERROR: Failed to close the file properly." << std::endl;
    //    return;
    //}
}

// CGProcRx
void USBInputSource::processReceive() {
    unsigned char* pBlkStart = rxBuffer;

    // Check if receive FIFO is empty
    if (readGSERegister(0) & 0x02) return;

    // First read a whole block of data from the FPGA
    // The buffer may be full, partly full, or empty
    // and it contains the frame marker at the start of each block.
    // The code skips that block and joins packets that span blocks.
    // After that, search for the sync marker
    // then get the 6-byte packet header
    // then get the rest of the packet
    

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

                // This appears to be writing telemetry to a file, but not doing anything with it.
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


// *********************************************************************/
// Function: CheckLinkStatus()
// Description: Check link status
// *********************************************************************/
void USBInputSource::checkLinkStatus(void)
{
	uint16_t r;

	// get GSE status
	r = readGSERegister(0); //(0x02);
	if ((r && 0x01) == 1)
	{
		//snprintf(StatusStr, sizeof(StatusStr), "FIFO Overflow                 ");
		std::cout << StatusStr << "checkLinkStatus: FIFO Overflow ************** " << std::endl;
	}

	//printf("\r%i\t%i\t%s", ctrTxPkts, ctrRxPkts, StatusStr);
    std::cout << "\r" << ctrTxPkts << "\t" << ctrRxPkts << "\t" << StatusStr << std::endl;

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

// stub - Alan's new code expects this
void GSEProcessPacket(uint16_t APID) {
    // need a char * pointer to reinterpret the packet data
    return;
}

