// commonFunctions.cpp

#include "commonFunctions.hpp"

extern ProgramState globalState;
extern std::mutex mtx;

// Function returns false if filename is empty
bool isValidFilename(const std::string& filename) {
    return !filename.empty();
}

// Utility function to create a single directory
bool create_single_directory(const std::string& path) {
    // Check if directory exists and create it if it doesn't
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        // Directory doesn't exist, create it
        if (mkdir(path.c_str(), 0755) != 0) {
            std::cerr << "ERROR: Could not create directory: " << path << std::endl;
            return false;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "ERROR: Path exists but is not a directory: " << path << std::endl;
        return false;
    }
    return true;
}

// Function to create directories recursively, like `mkdir -p`
bool create_directory_if_not_exists(const std::string& path) {
    std::istringstream pathStream(path);
    std::string segment;
    std::string currentPath;

    while (std::getline(pathStream, segment, '/')) {
        if (!segment.empty()) {
            currentPath += segment + "/";
            if (!create_single_directory(currentPath)) {
                return false;
            }
        }
    }

    return true;
}

// Convert 2d image into 1d image in transpose order for writing to a FITS image HDU
std::vector<uint16_t> transposeImageTo1D(const uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH]) {
    const uint32_t width = MEGS_IMAGE_WIDTH;
    const uint32_t height = MEGS_IMAGE_HEIGHT;
    std::vector<uint16_t> transposedData(width * height);

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t yoffset = y * width;
        for (uint32_t x = 0; x < width; ++x) {
            transposedData[x + yoffset] = image[y][x];
        }
    }
    return transposedData;
}

// reads a packet and writes it to the recordfile
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord) {
    std::vector<uint8_t> packet;
    int counter = 0;

    while (pktReader.readNextPacket(packet)) {
        counter++;
        //std::cout << "processPackets counter " << counter << std::endl;

        // Record packet if required
        // c++ guarantees evaluation order from left to right to support short-circuit evaluation
        if (!skipRecord && recordWriter && !recordWriter->writeSyncAndPacketToRecordFile(packet)) {
            LogFileWriter::getInstance().logError("ERROR: processPackets failed to write packet to record file.");
            return;
        }

        // Process packet
        processOnePacket(pktReader, packet);

    }
}

void processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet) {
    auto start = std::chrono::system_clock::now();

    auto header = std::vector<uint8_t>(packet.cbegin(), packet.cbegin() + PACKET_HEADER_SIZE);

    uint16_t apid = pktReader.getAPID(header);
    uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);
    uint16_t packetLength = pktReader.getPacketLength(header);

    auto payload = std::vector<uint8_t>(packet.cbegin() + PACKET_HEADER_SIZE, packet.cend());
    double timeStamp = pktReader.getPacketTimeStamp(payload);

    // if packets are being dropped, this will be a good place to check
    //LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
    //    sourceSequenceCounter, packetLength, timeStamp );

    switch (apid) {
        case MEGSA_APID:
            if ( sourceSequenceCounter == 0) {
                LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
                    sourceSequenceCounter, packetLength, timeStamp );
            }
            processMegsAPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSB_APID:
            if ( sourceSequenceCounter == 0) {
                LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
                    sourceSequenceCounter, packetLength, timeStamp );
            }
            processMegsBPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case ESP_APID:
            LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
                sourceSequenceCounter, packetLength, timeStamp );
            processESPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSP_APID:
            LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
                sourceSequenceCounter, packetLength, timeStamp );
            processMegsPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case HK_APID:
            LogFileWriter::getInstance().logInfo("APID {} SSC {} pktLen {} time {}", apid,
                sourceSequenceCounter, packetLength, timeStamp );
            processHKPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        default:
            LogFileWriter::getInstance().logError("APID {} SSC {} pktLen {} time {}", apid,
                sourceSequenceCounter, packetLength, timeStamp );
            std::cerr << "Unrecognized APID: " << apid << std::endl;
            // Handle error or unknown APID case if necessary
            globalState.packetsReceived.Unknown.fetch_add(1, std::memory_order_relaxed);
            break;
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;

    uint64_t elapsedMicrosec = 1.e6 * elapsed_seconds.count();
    LogFileWriter::getInstance().logInfo("processOnePacket for APID: {} Elapsed mics {}",apid, elapsedMicrosec);
}

// payloadBytesToUint32 creates a 32-bit int from 4 bytes in TAI time order starting at offsetByte
uint32_t payloadBytesToUint32(const std::vector<uint8_t>&payload, const int32_t offsetByte) {
    if (payload.size() < offsetByte + 4) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("payloadBytesToUint32 - Payload must contain at least 4 bytes.");
    }
    return (static_cast<uint32_t>(payload[offsetByte]) << 24) |
           (static_cast<uint32_t>(payload[offsetByte+1]) << 16) |
           (static_cast<uint32_t>(payload[offsetByte+2]) << 8)  |
           static_cast<uint32_t>(payload[offsetByte+3]);
}

uint32_t payloadToTAITimeSeconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4) {
        // Handle the error case, perhaps by throwing an exception
        // Iterate and print each value in hexadecimal format
        std::cout<<"ERROR: payloadToTAITimeSeconds - Payload is not long enough sizeof:"<<sizeof(payload)<<std::endl;
        std::cout<< "Payload: " << std::hex<<std::setw(2)<<std::setfill('0');
        for (uint8_t val : payload) {
            std::cout << " " << static_cast<int>(val) << " ";
        }
        std::cout << std::endl;
        
        throw std::invalid_argument("payloadToTAITimeSeconds - Payload must contain at least 4 bytes.");
    }

    uint32_t tai = (static_cast<uint32_t>(payload[0]) << 24) |
           (static_cast<uint32_t>(payload[1]) << 16) |
           (static_cast<uint32_t>(payload[2]) << 8)  |
           static_cast<uint32_t>(payload[3]);

    //std::cout << "payloadToTAITimeSeconds calculated " << tai << std::endl;

    return tai;
}

uint32_t payloadToTAITimeSubseconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 6) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("payloadToTAITimeSubseconds - Payload must contain at least 6 bytes.");
    }

    return (static_cast<uint32_t>(payload[5]) << 24) |
           (static_cast<uint32_t>(payload[4]) << 16);
}

// use template to allow any of the data structures to have the time variables populated
template <typename T>
void populateStructureTimes(T& oneStructure, const std::vector<uint8_t>& payload) {
    uint32_t tai_sec = payloadToTAITimeSeconds(payload);
    oneStructure.tai_time_seconds = tai_sec;
    oneStructure.tai_time_subseconds = payloadToTAITimeSubseconds(payload);

    // assign current tai time to firstpkt_tai_time_seconds and subseconds
    TimeInfo currentTime;
    currentTime.updateNow();
    oneStructure.rec_tai_seconds = currentTime.getTAISeconds();
    oneStructure.rec_tai_subseconds = currentTime.getTAISubseconds();

    uint16_t year, doy, hh, mm, ss;
    uint32_t sod;
    std::string iso8601;

    tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss, iso8601);
    
    oneStructure.sod = (uint32_t)sod;
    oneStructure.yyyydoy = (uint32_t)(year * 1000 + doy);
}

// Generic function to count saturated pixels in MEGS images
void countSaturatedPixels(const uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH],
                          uint32_t& saturatedPixelsTop,
                          uint32_t& saturatedPixelsBottom,
                          bool testPattern = false) {
    saturatedPixelsTop = 0;
    saturatedPixelsBottom = 0;

    //#pragma omp parallel for reduction(+:saturatedPixelsTop, saturatedPixelsBottom)
    for (uint32_t i = 0; i < MEGS_IMAGE_WIDTH; ++i) {
        for (uint32_t j = 0; j < MEGS_IMAGE_HEIGHT; ++j) {
            uint16_t maskedValue = image[j][i] & 0x3fff;
            if (maskedValue == 0x3fff) {
                if (j < MEGS_IMAGE_HEIGHT / 2) {
                    // Skip the test pattern case for the first pixel
                    if (!(testPattern && i == 0 && j == 0)) {
                        saturatedPixelsTop++;
                    }
                } else {
                    saturatedPixelsBottom++;
                }
            }
        }
    }
}

// the payload starts with the secondary header timestamp
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    //LogFileWriter::getInstance().logInfo("processMegsAPacket");

    // insert pixels into image

    //static std::string iso8601;

    static uint16_t processedPacketCounter=0;
    int8_t status=0;
    static bool testPattern;
    static bool isFirstImage = true;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC oneMEGSStructure;
    int vcdu_offset_to_sec_hdr = 20; // 6 bytes from packet start to timestamp, 8 byte IMPDU hdr, 6 byte CVCDU hdr is 20
    int32_t xpos, ypos;

    // payload starts at secondary header
    // 6-byte VCDU header, 8-byte IMPDU header, 6-byte primary packet header
    // data to copy starts at secondary header, includes 8-byte timestamp, 2 byte mode, then 2-byte pixel pairs (1752 bytes except for last packet)

    uint8_t vcdu[STANDARD_MEGSAB_PACKET_LENGTH + vcdu_offset_to_sec_hdr] = {0};

    // vcdu has 14 bytes before the packet start (vcdu=6, impdu=8)
    // pkthdr is 6 bytes before the payload starts at the 2nd hdr timestamp
    // copy from payload into vcdu starting at byte 20 from the start of sync, 16 from start of VCDU
    std::copy(payload.begin(), payload.end(), vcdu + vcdu_offset_to_sec_hdr);

    // vcdu should look like a fake VCDU, with garbage before the timestamp
    // and it does, just without the primary header, impdu hdr, and vcdu hdr

    if ((previousSrcSeqCount == -1) || (sourceSequenceCounter == 0) || 
        (sourceSequenceCounter <= previousSrcSeqCount)) {
        // packet is from a new image
        //std::string logMsg = "MA starting new image first SrcSeqCounter: " + std::to_string(sourceSequenceCounter);
        LogFileWriter::getInstance().logInfo("MA starting new image first SrcSeqCounter: {}", sourceSequenceCounter);
        //std::cout << "Info: " << logMsg << std::endl;

        //reset oneMEGSStructure
        oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 

        testPattern = false; // default is not a test pattern
        if ((vcdu[34] == 0) && (vcdu[35] == 2) && (vcdu[36] == 0) && (vcdu[37] == 1)) {
            testPattern = true; // use vcdu[34]==0 vcdu[35]=2 vcdu[36]=0 vcdu[37]=1 to identify TP (first 2 pixesl are bad so skip 30-33)
            std::cout << "processMegsAPacket identified a test pattern" <<std::endl;
        }

        populateStructureTimes(oneMEGSStructure, payload);

        mtx.lock();
        globalState.megsa.tai_time_seconds = oneMEGSStructure.tai_time_seconds;
        globalState.megsa.tai_time_subseconds = oneMEGSStructure.tai_time_subseconds;
        globalState.megsa.sod = oneMEGSStructure.sod;
        globalState.megsa.yyyydoy = oneMEGSStructure.yyyydoy;

        std::memcpy(globalState.megsAPayloadBytes, payload.data(), payload.size());

        mtx.unlock();

        processedPacketCounter=0;
    }
    if ((!isFirstImage) && (((previousSrcSeqCount + 1) % N_PKT_PER_IMAGE) != sourceSequenceCounter)) {
        // there is a gap in the data
        LogFileWriter::getInstance().logError("MA packet out of sequence SSC: {} previous SSC: {}", sourceSequenceCounter, previousSrcSeqCount);
        std::cout << "processMegsAPacket - data gap SSC: " << sourceSequenceCounter << " previous SSC: " << previousSrcSeqCount << std::endl;

        globalState.dataGapsMA.fetch_add(1, std::memory_order_relaxed);

    }
    previousSrcSeqCount = sourceSequenceCounter;

    // begin assigning data into oneMEGSStructure

    // assing pixel values from the packet into the proper locations in the image
    // The oneMEGSStructure is the one that is written to FITS and is initialized to 0
    int parityErrors = assemble_image(vcdu, &oneMEGSStructure, sourceSequenceCounter, testPattern, xpos, ypos, &status);

    {
        globalState.packetsReceived.MA.fetch_add(1);
        globalState.isFirstMAImage.store(isFirstImage, std::memory_order_relaxed);
        // The globalState.megsa image is NOT initialized and just overwrites each packet location as it is received
        globalState.parityErrorsMA.fetch_add(assemble_image(vcdu, &globalState.megsa, sourceSequenceCounter, testPattern, xpos, ypos, &status), std::memory_order_relaxed);
        if ((processedPacketCounter % IMAGE_UPDATE_INTERVAL) == 0) {
            globalState.megsAUpdated.store(true, std::memory_order_relaxed);
            globalState.MAypos.store(ypos, std::memory_order_relaxed); // ypos is atomic
            // count saturated pixels
            uint32_t saturatedPixelsTop, saturatedPixelsBottom;
            mtx.lock();
            countSaturatedPixels(globalState.megsa.image,
                    saturatedPixelsTop,
                    saturatedPixelsBottom,
                    testPattern);
            mtx.unlock();
            globalState.saturatedPixelsMABottom.store(saturatedPixelsBottom, std::memory_order_relaxed);
            globalState.saturatedPixelsMATop.store(saturatedPixelsTop, std::memory_order_relaxed);
        }
    }

    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MA parity errors: {} SSC: {}", parityErrors, sourceSequenceCounter);
        //std::cout << "processMegsAPacket - assemble_image returned parity errors: " << parityErrors << " ssc:"<<sourceSequenceCounter<<"\n";
    }
    processedPacketCounter++; // count packets processed

    if ( sourceSequenceCounter == 2394) {
        //std::cout<<"end of MEGS-A at 2394"<<"\n";
        LogFileWriter::getInstance().logInfo("end of MEGS-A image ssc=2394");

        isFirstImage = false;

        // may need to run this in another thread

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        //std::cout << "processMegsAPacket number of packets: " << processedPacketCounter << std::endl;
        if (fitsFileWriter) {
            //std::cout << "processMegsAPacket: tai_time_seconds = " << oneMEGSStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeMegsAFITS( oneMEGSStructure )) {
                LogFileWriter::getInstance().logInfo("writeMegsAFITS write error");
                std::cout << "ERROR: writeMegsAFITS returned an error" << std::endl;
            }
            processedPacketCounter=0;
            // reset the structure immediately after writing
            oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 
        }
    }
}

void processMegsBPacket(std::vector<uint8_t> payload, uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    //LogFileWriter::getInstance().logInfo("processMegsBPacket");

    // insert pixels into image

    //static std::string iso8601;

    static uint16_t processedPacketCounter=0;
    int8_t status=0;
    static bool testPattern;
    static bool isFirstImage = true;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC oneMEGSStructure;
    int32_t xpos, ypos;

    int vcdu_offset_to_sec_hdr = 20; // 6 bytes from packet start to timestamp, 8 byte IMPDU hdr, 6 byte CVCDU hdr is 20

    // payload starts at secondary header
    // 6-byte VCDU header, 8-byte IMPDU header, 6-byte primary packet header
    // data to copy starts at secondary header, includes 8-byte timestamp, 2 byte mode, then 2-byte pixel pairs (1752 bytes except for last packet)

    uint8_t vcdu[STANDARD_MEGSAB_PACKET_LENGTH + vcdu_offset_to_sec_hdr] = {0};

    // vcdu has 14 bytes before the packet start (vcdu=6, impdu=8)
    // pkthdr=6 bytes before the payload starts at the 2nd hdr timestamp
    // copy from payload into vcdu starting at byte 20 from the start of sync, 16 from start of VCDU
    std::copy(payload.begin(), payload.end(), vcdu + vcdu_offset_to_sec_hdr);

    if ((previousSrcSeqCount == -1) || (sourceSequenceCounter == 0) || 
        (sourceSequenceCounter <= previousSrcSeqCount)) {
        // packet is from a new image
        //std::string logMsg = "MB starting new image first SrcSeqCounter: " + std::to_string(sourceSequenceCounter);
        LogFileWriter::getInstance().logInfo("MB starting new image first SrcSeqCounter: {}", sourceSequenceCounter);
        //std::cout << "Info: " << logMsg << std::endl;

        //reset oneMEGSStructure
        oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 

        testPattern = false; // default is not a test pattern
        if ((vcdu[34] == 0x8f) && (vcdu[35] == 0xfc) && (vcdu[36] == 0x87) && (vcdu[37] == 0xfe)) { //changed for MEGS-B
            testPattern = true; // identify TP (first 2 pixels are bad so skip 30-33)
            // David sets the first pixels to ff ff aa aa - these 2 pixels fail parity
            std::cout << "processMegsBPacket identified a test pattern" <<std::endl;
        }

        //TODO: can this stuff be replaced with populateStructureTimes?

        populateStructureTimes(oneMEGSStructure, payload);

        // only assign the time from the first packet, the rest keep changing

        mtx.lock();
        globalState.megsb.tai_time_seconds = oneMEGSStructure.tai_time_seconds;
        globalState.megsb.tai_time_subseconds = oneMEGSStructure.tai_time_subseconds;
        globalState.megsb.sod = oneMEGSStructure.sod;
        globalState.megsb.yyyydoy = oneMEGSStructure.yyyydoy;

        std::memcpy(globalState.megsBPayloadBytes, payload.data(), payload.size());

        mtx.unlock();

        processedPacketCounter=0;
    }
    if ((!isFirstImage) && (((previousSrcSeqCount + 1) % N_PKT_PER_IMAGE) != sourceSequenceCounter)) {
        LogFileWriter::getInstance().logError("MEGS-B packet out of sequence SSC: {} {}", previousSrcSeqCount, sourceSequenceCounter);
        std::cout << "MEGS-B packet out of sequence: " << previousSrcSeqCount << " " << sourceSequenceCounter << std::endl;
        globalState.dataGapsMB.fetch_add(1, std::memory_order_relaxed);
    }
    previousSrcSeqCount = sourceSequenceCounter;

    // begin assigning data into oneMEGSStructure

    // assing pixel values from the packet into the proper locations in the image
    int parityErrors = assemble_image(vcdu, &oneMEGSStructure, sourceSequenceCounter, testPattern, xpos, ypos, &status);

    {
        globalState.packetsReceived.MB.fetch_add(1);
        if (isFirstImage) {
            if (globalState.packetsReceived.MB.load() > 2395) {
                isFirstImage = false;
            }
        }
        globalState.isFirstMBImage.store(isFirstImage, std::memory_order_relaxed);
        // The globalState.megsa image is NOT re-initialized and just overwrites each packet location as it is received
        globalState.parityErrorsMB.fetch_add(assemble_image(vcdu, &globalState.megsb, sourceSequenceCounter, testPattern, xpos, ypos, &status), std::memory_order_relaxed);

        if ((processedPacketCounter % IMAGE_UPDATE_INTERVAL) == 0) {
            globalState.megsBUpdated.store(true, std::memory_order_relaxed);
            globalState.MBypos.store(ypos, std::memory_order_relaxed);  // MBypos is atomic
            uint32_t saturatedPixelsTop, saturatedPixelsBottom;
            mtx.lock();
            countSaturatedPixels(globalState.megsb.image,
                    saturatedPixelsTop,
                    saturatedPixelsBottom,
                    testPattern);
            mtx.unlock();
            globalState.saturatedPixelsMBBottom.store(saturatedPixelsBottom, std::memory_order_relaxed);
            globalState.saturatedPixelsMBTop.store(saturatedPixelsTop, std::memory_order_relaxed);
        }
    }
    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MB parity errors: {} in SSC:", parityErrors,sourceSequenceCounter);
        //std::cout << "processMegsBPacket - assemble_image returned parity errors: " << parityErrors << " ssc:"<<sourceSequenceCounter<<"\n";
    }

    processedPacketCounter++; // count packets processed

    if ( sourceSequenceCounter == 2394) {
        //std::cout<<"end of MEGS-B at 2394"<<"\n";
        LogFileWriter::getInstance().logInfo("end of MEGS-B image ssc=2394");

        isFirstImage = false;

        // may need to run this in another thread

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        //std::cout << "processMegsBPacket number of packets: " << processedPacketCounter << std::endl;
        if (fitsFileWriter) {
            //std::cout << "procesMegsBPacket: tai_time_seconds = " << oneMEGSStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeMegsBFITS( oneMEGSStructure )) {
                LogFileWriter::getInstance().logInfo("writeMegsBFITS write error");
                std::cout << "ERROR: writeMegsBFITS returned an error" << std::endl;
            }
            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 
        }
    }
}

// MEGS-P packet
void processMegsPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
    
    MEGSP_PACKET oneMEGSPStructure = {0};
    static uint16_t processedPacketCounter = 0;
    static uint16_t lastSourceSequenceCounter = sourceSequenceCounter - 1;

    if ( ((lastSourceSequenceCounter + 1) % 16384) != sourceSequenceCounter) {
        LogFileWriter::getInstance().logError("MEGS-P packet out of sequence: {} {}", lastSourceSequenceCounter, sourceSequenceCounter);
        std::cout << "MEGS-P packet out of sequence: " << lastSourceSequenceCounter << " " << sourceSequenceCounter << std::endl;
        globalState.dataGapsMP.fetch_add(1, std::memory_order_relaxed);
    }
    lastSourceSequenceCounter = sourceSequenceCounter;

    //LogFileWriter::getInstance().logInfo("processMegsPPacket");
    //std::cout<<"processMegsPPacket 1" << std::endl;

    populateStructureTimes(oneMEGSPStructure, payload);

    int firstbyteoffset = 10; //time=8,mode=2

    int packetoffset = processedPacketCounter * MEGSP_INTEGRATIONS_PER_PACKET;
    constexpr int bytesPerIntegration = (2 * 2); // 2 bytes per diode, 2 diodes
    for (int i=0; i<MEGSP_INTEGRATIONS_PER_PACKET; ++i) {

        int incr = (i*bytesPerIntegration) + firstbyteoffset; // 4 is bytes per integration, 2 bytes per diode * 2 diodes per integration
        int index = packetoffset + i;
        //std::cout<<"processMegsPPacket 2l " << i << " "<<incr << " "<<index << " "<< sizeof(payload)<<std::endl;

        oneMEGSPStructure.MP_lya[index] = (uint16_t (payload[incr]) << 8) | (uint16_t (payload[incr + 1]));
        oneMEGSPStructure.MP_dark[index] = (uint16_t (payload[incr+2]) << 8) | (uint16_t (payload[incr+3]));
        mtx.lock();
        globalState.megsp.MP_lya[index] = oneMEGSPStructure.MP_lya[index];
        globalState.megsp.MP_dark[index] = oneMEGSPStructure.MP_dark[index];
        mtx.unlock();
    }

    processedPacketCounter++;

    {
        globalState.packetsReceived.MP.fetch_add(1);
        mtx.lock();
        std::memcpy(globalState.megsPPayloadBytes, payload.data(), payload.size());
        mtx.unlock();
    }

    // ONLY WRITE WHEN STRUCTURE IS FULL
    if ( processedPacketCounter == MEGSP_PACKETS_PER_FILE ) {
        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();

        if (fitsFileWriter) {
            //std::cout << "procesMegsPPacket: tai_time_seconds = " << oneMEGSPStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeMegsPFITS( oneMEGSPStructure )) {
                LogFileWriter::getInstance().logInfo("writeMegsPFITS write error");
                std::cout << "ERROR: writeMegsPFITS returned an error" << std::endl;
            }
            //std::cout<<"processMegsPPacket - MP_lya values" << std::endl;
            //printBytes(oneMEGSPStructure.MP_lya,19);

            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneMEGSPStructure = MEGSP_PACKET{0}; // c++11 
        }
    }
}

void processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    ESP_PACKET oneESPStructure = {0};
    static uint16_t processedPacketCounter = 0;
    static uint16_t lastSourceSequenceCounter = sourceSequenceCounter - 1;
    static long dataGapsESP = 0;

    if ( ((lastSourceSequenceCounter + 1) % 16384) != sourceSequenceCounter) {
        LogFileWriter::getInstance().logError("ESP packet out of sequence: {} {}", lastSourceSequenceCounter, sourceSequenceCounter);
        std::cout << "ESP packet out of sequence: " << lastSourceSequenceCounter << " " << sourceSequenceCounter << std::endl;
        dataGapsESP++;
        }
    lastSourceSequenceCounter = sourceSequenceCounter;

    //printBytes(&payload[0], 40);

    populateStructureTimes(oneESPStructure, payload);
    
    int firstbyteoffset = 10;

    int packetoffset = processedPacketCounter * ESP_INTEGRATIONS_PER_PACKET;
    //std::cout<<"processESPPacket packetoffset "<< packetoffset << std::endl;
    // integrations are sequentially adjacent in the packet
    // pri hdr, sec hdr, mode, integration 1, integrtion 2, etc
    // each ESP integration starts with a 2 byte counter, then 9 2 byte diode measurements
    constexpr int bytesperintegration = (2 * 9) + 2; // 9 diodes and one counter, is 20
    for (int i=0; i<ESP_INTEGRATIONS_PER_PACKET; ++i) {
        int incr = (i*bytesperintegration) + firstbyteoffset;
        int index = packetoffset + i;
        oneESPStructure.ESP_xfer_cnt[index] = (uint16_t (payload[incr]) << 8) | (uint16_t (payload[incr + 1]));
        oneESPStructure.ESP_q0[index] = (uint16_t (payload[incr+2]) << 8) | (uint16_t (payload[incr+3]));
        oneESPStructure.ESP_q1[index] = (uint16_t (payload[incr+4]) << 8) | (uint16_t (payload[incr+5]));
        oneESPStructure.ESP_q2[index] = (uint16_t (payload[incr+6]) << 8) | (uint16_t (payload[incr+7]));
        oneESPStructure.ESP_q3[index] = (uint16_t (payload[incr+8]) << 8) | (uint16_t (payload[incr+9]));
        oneESPStructure.ESP_171[index] = (uint16_t (payload[incr+10]) << 8) | (uint16_t (payload[incr+11]));
        oneESPStructure.ESP_257[index] = (uint16_t (payload[incr+12]) << 8) | (uint16_t (payload[incr+13]));
        oneESPStructure.ESP_304[index] = (uint16_t (payload[incr+14]) << 8) | (uint16_t (payload[incr+15]));
        oneESPStructure.ESP_366[index] = (uint16_t (payload[incr+16]) << 8) | (uint16_t (payload[incr+17]));
        oneESPStructure.ESP_dark[index] = (uint16_t (payload[incr+18]) << 8) | (uint16_t (payload[incr+19]));

        mtx.lock();
        globalState.esp.ESP_xfer_cnt[index] = oneESPStructure.ESP_xfer_cnt[index];
        globalState.esp.ESP_q0[index] = oneESPStructure.ESP_q0[index];
        globalState.esp.ESP_q1[index] = oneESPStructure.ESP_q1[index];
        globalState.esp.ESP_q2[index] = oneESPStructure.ESP_q2[index];
        globalState.esp.ESP_q3[index] = oneESPStructure.ESP_q3[index];
        globalState.esp.ESP_171[index] = oneESPStructure.ESP_171[index];
        globalState.esp.ESP_257[index] = oneESPStructure.ESP_257[index];
        globalState.esp.ESP_304[index] = oneESPStructure.ESP_304[index];
        globalState.esp.ESP_366[index] = oneESPStructure.ESP_366[index];
        globalState.esp.ESP_dark[index] = oneESPStructure.ESP_dark[index];
        mtx.unlock();

        //std::cout<<"ESP q0: "<< oneESPStructure.ESP_q0[index] << std::endl;
    }


    processedPacketCounter++;

    {
        globalState.packetsReceived.ESP.fetch_add(1, std::memory_order_relaxed);

        globalState.dataGapsESP.fetch_add(dataGapsESP, std::memory_order_relaxed);

        mtx.lock();
        globalState.esp.rec_tai_seconds = oneESPStructure.tai_time_seconds;
        globalState.esp.rec_tai_subseconds = oneESPStructure.tai_time_subseconds;
        std::memcpy(globalState.espPayloadBytes, payload.data(), payload.size());
        mtx.unlock();
        dataGapsESP = 0; // reset to zero
    }


    // ONLY WRITE WHEN STRUCTURE IS FULL
    if ( processedPacketCounter == ESP_PACKETS_PER_FILE ) {

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();

        if (fitsFileWriter) {
            //std::cout << "procesESPPacket: tai_time_seconds = " << oneESPStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeESPFITS( oneESPStructure )) {
                LogFileWriter::getInstance().logInfo("writeESPFITS write error");
                std::cout << "ERROR: writESPFITS returned an error" << std::endl;
            }
            //std::cout<<"processESPPacket - ESP diode 0 values" << std::endl;
            //printBytes(oneESPStructure.ESP_q0,19);

            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneESPStructure = ESP_PACKET{0}; // c++11 
        }
    }

}

void processHKPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
    
    return; // Need to debug

    SHK_PACKET oneSHKStructure = {0};
    static uint16_t processedPacketCounter=0;
    static uint16_t lastSourceSequenceCounter = sourceSequenceCounter - 1;

    if ( ((lastSourceSequenceCounter + 1) % 16384) != sourceSequenceCounter) {
        LogFileWriter::getInstance().logError("SHK packet out of sequence: {} {}", lastSourceSequenceCounter, sourceSequenceCounter);
        std::cout << "SHK packet out of sequence: " << lastSourceSequenceCounter << " " << sourceSequenceCounter << std::endl;
        globalState.dataGapsSHK.fetch_add(1, std::memory_order_relaxed);
    }

    populateStructureTimes(oneSHKStructure, payload);

    int firstbyteoffset = 10; // offset into payload after 4 bytes TAI sec, 4 bytes TAIsubsec, 2 bytes modeword

    int packetoffset = processedPacketCounter * SHK_INTEGRATIONS_PER_PACKET;
    //std::cout<<"processESPPacket packetoffset "<< packetoffset << std::endl;
    // integrations are sequentially adjacent in the packet
    // pri hdr, sec hdr, mode, integration 1, integrtion 2, etc
    // each SHK "integration" starts with a 2 byte counter, then 9 2 byte diode measurements
    constexpr int bytesperintegration = (4 * 65) + 2; // 65 32-bit values, some are unused
    for (int i=0; i<SHK_INTEGRATIONS_PER_PACKET; ++i) {
        int incr = (i*bytesperintegration) + firstbyteoffset;
        int index = packetoffset + i;
        oneSHKStructure.FPGA_Board_Temperature[index] = payloadBytesToUint32(payload, incr+4);
        oneSHKStructure.FPGA_Board_p5_0_Voltage[index] = payloadBytesToUint32(payload, incr+8);
        oneSHKStructure.FPGA_Board_p3_3_Voltage[index] = payloadBytesToUint32(payload, incr+12);
        oneSHKStructure.FPGA_Board_p2_5_Voltage[index] = payloadBytesToUint32(payload, incr+16);
        oneSHKStructure.FPGA_Board_p1_2_Voltage[index] = payloadBytesToUint32(payload, incr+20);
        oneSHKStructure.MEGSA_CEB_Temperature[index] = payloadBytesToUint32(payload, incr+24);
        oneSHKStructure.MEGSA_CPR_Temperature[index] = payloadBytesToUint32(payload, incr+28);
        oneSHKStructure.MEGSA_p24_Voltage[index] = payloadBytesToUint32(payload, incr+32);
        oneSHKStructure.MEGSA_p15_Voltage[index] = payloadBytesToUint32(payload, incr+36);
        oneSHKStructure.MEGSA_m15_Voltage[index] = payloadBytesToUint32(payload, incr+40); // 10*4
        oneSHKStructure.MEGSA_p5_0_Analog_Voltage[index] = payloadBytesToUint32(payload, incr+44);
        oneSHKStructure.MEGSA_m5_0_Voltage[index] = payloadBytesToUint32(payload, incr+48);
        oneSHKStructure.MEGSA_p5_0_Digital_Voltage[index] = payloadBytesToUint32(payload, incr+52);
        oneSHKStructure.MEGSA_p2_5_Voltage[index] = payloadBytesToUint32(payload, incr+56);
        oneSHKStructure.MEGSA_p24_Current[index] = payloadBytesToUint32(payload, incr+60);
        oneSHKStructure.MEGSA_p15_Current[index] = payloadBytesToUint32(payload, incr+64);
        oneSHKStructure.MEGSA_m15_Current[index] = payloadBytesToUint32(payload, incr+68);
        oneSHKStructure.MEGSA_p5_0_Analog_Current[index] = payloadBytesToUint32(payload, incr+72);
        oneSHKStructure.MEGSA_m5_0_Current[index] = payloadBytesToUint32(payload, incr+76);
        oneSHKStructure.MEGSA_p5_0_Digital_Current[index] = payloadBytesToUint32(payload, incr+80); //20*4
        oneSHKStructure.MEGSA_p2_5_Current[index] = payloadBytesToUint32(payload, incr+84);
        oneSHKStructure.MEGSA_Integration_Register[index] = payloadBytesToUint32(payload, incr+88);
        oneSHKStructure.MEGSA_Analog_Mux_Register[index] = payloadBytesToUint32(payload, incr+92);
        oneSHKStructure.MEGSA_Digital_Status_Register[index] = payloadBytesToUint32(payload, incr+96);
        oneSHKStructure.MEGSA_Integration_Timer_Register[index] = payloadBytesToUint32(payload, incr+100);
        oneSHKStructure.MEGSA_Command_Error_Count_Register[index] = payloadBytesToUint32(payload, incr+104);
        oneSHKStructure.MEGSA_CEB_FPGA_Version_Register[index] = payloadBytesToUint32(payload, incr+108);
        // gap of 4 32-bit values
        oneSHKStructure.MEGSB_CEB_Temperature[index] = payloadBytesToUint32(payload, incr+128);
        oneSHKStructure.MEGSB_CPR_Temperature[index] = payloadBytesToUint32(payload, incr+132);
        oneSHKStructure.MEGSB_p24_Voltage[index] = payloadBytesToUint32(payload, incr+136);
        oneSHKStructure.MEGSB_p15_Voltage[index] = payloadBytesToUint32(payload, incr+140);
        oneSHKStructure.MEGSB_m15_Voltage[index] = payloadBytesToUint32(payload, incr+144);
        oneSHKStructure.MEGSB_p5_0_Analog_Voltage[index] = payloadBytesToUint32(payload, incr+148);
        oneSHKStructure.MEGSB_m5_0_Voltage[index] = payloadBytesToUint32(payload, incr+152);
        oneSHKStructure.MEGSB_p5_0_Digital_Voltage[index] = payloadBytesToUint32(payload, incr+156);
        oneSHKStructure.MEGSB_p2_5_Voltage[index] = payloadBytesToUint32(payload, incr+160); // 40*4
        oneSHKStructure.MEGSB_p24_Current[index] = payloadBytesToUint32(payload, incr+164);
        oneSHKStructure.MEGSB_p15_Current[index] = payloadBytesToUint32(payload, incr+168);
        oneSHKStructure.MEGSB_m15_Current[index] = payloadBytesToUint32(payload, incr+172);
        oneSHKStructure.MEGSB_p5_0_Analog_Current[index] = payloadBytesToUint32(payload, incr+176);
        oneSHKStructure.MEGSB_m5_0_Current[index] = payloadBytesToUint32(payload, incr+180);
        oneSHKStructure.MEGSB_p5_0_Digital_Current[index] = payloadBytesToUint32(payload, incr+184);
        oneSHKStructure.MEGSB_p2_5_Current[index] = payloadBytesToUint32(payload, incr+188);
        oneSHKStructure.MEGSB_Integration_Register[index] = payloadBytesToUint32(payload, incr+192);
        oneSHKStructure.MEGSB_Analog_Mux_Register[index] = payloadBytesToUint32(payload, incr+196);
        oneSHKStructure.MEGSB_Digital_Status_Register[index] = payloadBytesToUint32(payload, incr+200); // 50*4
        oneSHKStructure.MEGSB_Integration_Timer_Register[index] = payloadBytesToUint32(payload, incr+204);
        oneSHKStructure.MEGSB_Command_Error_Count_Register[index] = payloadBytesToUint32(payload, incr+208);
        oneSHKStructure.MEGSB_CEB_FPGA_Version_Register[index] = payloadBytesToUint32(payload, incr+212);

        oneSHKStructure.MEGSA_Thermistor_Diode[index] = payloadBytesToUint32(payload, incr+216);
        oneSHKStructure.MEGSA_PRT[index] = payloadBytesToUint32(payload, incr+220);
        oneSHKStructure.MEGSB_Thermistor_Diode[index] = payloadBytesToUint32(payload, incr+224);
        oneSHKStructure.MEGSB_PRT[index] = payloadBytesToUint32(payload, incr+228);
        // additional 4 spares at the end

        mtx.lock();
        globalState.shk.FPGA_Board_Temperature[index] = oneSHKStructure.FPGA_Board_Temperature[index];
        globalState.shk.FPGA_Board_p5_0_Voltage[index] = oneSHKStructure.FPGA_Board_p5_0_Voltage[index];
        globalState.shk.FPGA_Board_p3_3_Voltage[index] = oneSHKStructure.FPGA_Board_p3_3_Voltage[index];
        globalState.shk.FPGA_Board_p2_5_Voltage[index] = oneSHKStructure.FPGA_Board_p2_5_Voltage[index];
        globalState.shk.FPGA_Board_p1_2_Voltage[index] = oneSHKStructure.FPGA_Board_p1_2_Voltage[index];
        globalState.shk.MEGSA_CEB_Temperature[index] = oneSHKStructure.MEGSA_CEB_Temperature[index];
        globalState.shk.MEGSA_CPR_Temperature[index] = oneSHKStructure.MEGSA_CPR_Temperature[index];
        globalState.shk.MEGSA_p24_Voltage[index] = oneSHKStructure.MEGSA_p24_Voltage[index];
        globalState.shk.MEGSA_p15_Voltage[index] = oneSHKStructure.MEGSA_p15_Voltage[index];
        globalState.shk.MEGSA_m15_Voltage[index] = oneSHKStructure.MEGSA_m15_Voltage[index];
        globalState.shk.MEGSA_p5_0_Analog_Voltage[index] = oneSHKStructure.MEGSA_p5_0_Analog_Voltage[index];
        globalState.shk.MEGSA_m5_0_Voltage[index] = oneSHKStructure.MEGSA_m5_0_Voltage[index];
        globalState.shk.MEGSA_p5_0_Digital_Voltage[index] = oneSHKStructure.MEGSA_p5_0_Digital_Voltage[index];
        globalState.shk.MEGSA_p2_5_Voltage[index] = oneSHKStructure.MEGSA_p2_5_Voltage[index];
        globalState.shk.MEGSA_p24_Current[index] = oneSHKStructure.MEGSA_p24_Current[index];
        globalState.shk.MEGSA_p15_Current[index] = oneSHKStructure.MEGSA_p15_Current[index];
        globalState.shk.MEGSA_m15_Current[index] = oneSHKStructure.MEGSA_m15_Current[index];
        globalState.shk.MEGSA_p5_0_Analog_Current[index] = oneSHKStructure.MEGSA_p5_0_Analog_Current[index];
        globalState.shk.MEGSA_m5_0_Current[index] = oneSHKStructure.MEGSA_m5_0_Current[index];
        globalState.shk.MEGSA_p5_0_Digital_Current[index] = oneSHKStructure.MEGSA_p5_0_Digital_Current[index];
        globalState.shk.MEGSA_p2_5_Current[index] = oneSHKStructure.MEGSA_p2_5_Current[index];
        globalState.shk.MEGSA_Integration_Register[index] = oneSHKStructure.MEGSA_Integration_Register[index];
        globalState.shk.MEGSA_Analog_Mux_Register[index] = oneSHKStructure.MEGSA_Analog_Mux_Register[index];
        globalState.shk.MEGSA_Digital_Status_Register[index] = oneSHKStructure.MEGSA_Digital_Status_Register[index];
        globalState.shk.MEGSA_Integration_Timer_Register[index] = oneSHKStructure.MEGSA_Integration_Timer_Register[index];
        globalState.shk.MEGSA_Command_Error_Count_Register[index] = oneSHKStructure.MEGSA_Command_Error_Count_Register[index];
        globalState.shk.MEGSA_CEB_FPGA_Version_Register[index] = oneSHKStructure.MEGSA_CEB_FPGA_Version_Register[index];
        globalState.shk.MEGSB_CEB_Temperature[index] = oneSHKStructure.MEGSB_CEB_Temperature[index];
        globalState.shk.MEGSB_CPR_Temperature[index] = oneSHKStructure.MEGSB_CPR_Temperature[index];
        globalState.shk.MEGSB_p24_Voltage[index] = oneSHKStructure.MEGSB_p24_Voltage[index];
        globalState.shk.MEGSB_p15_Voltage[index] = oneSHKStructure.MEGSB_p15_Voltage[index];
        globalState.shk.MEGSB_m15_Voltage[index] = oneSHKStructure.MEGSB_m15_Voltage[index];
        globalState.shk.MEGSB_p5_0_Analog_Voltage[index] = oneSHKStructure.MEGSB_p5_0_Analog_Voltage[index];
        globalState.shk.MEGSB_m5_0_Voltage[index] = oneSHKStructure.MEGSB_m5_0_Voltage[index];
        globalState.shk.MEGSB_p5_0_Digital_Voltage[index] = oneSHKStructure.MEGSB_p5_0_Digital_Voltage[index];
        globalState.shk.MEGSB_p2_5_Voltage[index] = oneSHKStructure.MEGSB_p2_5_Voltage[index];
        globalState.shk.MEGSB_p24_Current[index] = oneSHKStructure.MEGSB_p24_Current[index];
        globalState.shk.MEGSB_p15_Current[index] = oneSHKStructure.MEGSB_p15_Current[index];
        globalState.shk.MEGSB_m15_Current[index] = oneSHKStructure.MEGSB_m15_Current[index];
        globalState.shk.MEGSB_p5_0_Analog_Current[index] = oneSHKStructure.MEGSB_p5_0_Analog_Current[index];
        globalState.shk.MEGSB_m5_0_Current[index] = oneSHKStructure.MEGSB_m5_0_Current[index];
        globalState.shk.MEGSB_p5_0_Digital_Current[index] = oneSHKStructure.MEGSB_p5_0_Digital_Current[index];
        globalState.shk.MEGSB_p2_5_Current[index] = oneSHKStructure.MEGSB_p2_5_Current[index];
        globalState.shk.MEGSB_Integration_Register[index] = oneSHKStructure.MEGSB_Integration_Register[index];
        globalState.shk.MEGSB_Analog_Mux_Register[index] = oneSHKStructure.MEGSB_Analog_Mux_Register[index];
        globalState.shk.MEGSB_Digital_Status_Register[index] = oneSHKStructure.MEGSB_Digital_Status_Register[index];
        globalState.shk.MEGSB_Integration_Timer_Register[index] = oneSHKStructure.MEGSB_Integration_Timer_Register[index];
        globalState.shk.MEGSB_Command_Error_Count_Register[index] = oneSHKStructure.MEGSB_Command_Error_Count_Register[index];
        globalState.shk.MEGSB_CEB_FPGA_Version_Register[index] = oneSHKStructure.MEGSB_CEB_FPGA_Version_Register[index];
        globalState.shk.MEGSA_Thermistor_Diode[index] = oneSHKStructure.MEGSA_Thermistor_Diode[index];
        globalState.shk.MEGSA_PRT[index] = oneSHKStructure.MEGSA_PRT[index];
        globalState.shk.MEGSB_Thermistor_Diode[index] = oneSHKStructure.MEGSB_Thermistor_Diode[index];
        globalState.shk.MEGSB_PRT[index] = oneSHKStructure.MEGSB_PRT[index];
        mtx.unlock();

    }

    processedPacketCounter++;

    {
        globalState.packetsReceived.SHK.fetch_add(1);
        mtx.lock();        
        std::memcpy(globalState.shkPayloadBytes, payload.data(), payload.size());
        mtx.unlock();
    }

    // ONLY WRITE WHEN STRUCTURE IS FULL
    if ( processedPacketCounter == SHK_PACKETS_PER_FILE ) {
        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();

        if (fitsFileWriter) {
            std::cout << "procesESPPacket: tai_time_seconds = " << oneSHKStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeSHKFITS( oneSHKStructure )) {
                LogFileWriter::getInstance().logInfo("writeSHKFITS write error");
                std::cout << "ERROR: writeSHKFITS returned an error" << std::endl;
            }
            //std::cout<<"processSHKPacket - SHK FPGA_Board_Temperature values" << std::endl;
            //printBytes(oneSHKStructure.FPGA_Board_Temperature,19);

            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneSHKStructure = SHK_PACKET{0}; // c++11 
        }
    }
}

void printBytesToStdOut(const uint8_t* array, uint32_t start, uint32_t end) {
    std::cout << std::hex << std::setfill('0'); // Set fill character to '0' for uniform width

    // Loop through the array from start to end
    for (uint32_t i = start; i <= end; ++i) {
        // Print each value as 2-digit hex with a space separator
        std::cout << std::setw(2) << static_cast<int>(array[i]) << " ";
    }
    std::cout << std::dec << std::endl; // Reset to decimal output
}

void printBytes(const void* ptr, size_t size) {
    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytePtr[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Function to print the first 'count' elements of a uint16_t array in hexadecimal
void printUint16ToStdOut(const uint16_t* image, size_t size, size_t count) {
    // Ensure the array has at least 'count' elements, or print as many as available
    size_t limit = std::min(size, count);

    // Print each element in hex with leading zeros and a width of 4
    std::cout << "printUint16ToStdOut hex " << ":" << std::setw(4) << std::setfill('0') << std::hex;
    for (size_t i = 0; i < limit; ++i) {
        std::cout << " " << image[i];
    }

    // Reset output to decimal format (optional)
    std::cout << std::dec << std::endl;
}