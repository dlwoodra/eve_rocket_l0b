// commonFunctions.cpp

#include "commonFunctions.hpp"
//#include "eve_l0b.hpp" // included in commonFunctions.hpp

// Function returns false if filename is empty
bool isValidFilename(const std::string& filename) {
    return !filename.empty();
}

// reads a packet and writes it to the recordfile
void processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord) {
    std::vector<uint8_t> packet;
    int counter = 0;

    std::cout << "entered processPackets" << std::endl;
    while (pktReader.readNextPacket(packet)) {
        counter++;
        //std::cout << "processPackets counter " << counter << std::endl;

        // Record packet if required
        // c++ guarantees evaluation order from left to right to support short-circuit evaluation
        if (!skipRecord && recordWriter && !recordWriter->writeSyncAndPacketToRecordFile(packet)) {
            LogFileWriter::getInstance().logError("ERROR: processPackets failed to write packet to record file.");
            return;
        } else {
            // this next line generates too many message to the screen
            //std::cout << "processPackets wrote to recordFilename " << recordWriter->getRecordFilename() << std::endl;
        }

        // Process packet
        processOnePacket(pktReader, packet);
    }
}

void processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet) {
    auto start = std::chrono::system_clock::now();

    auto header = std::vector<uint8_t>(packet.cbegin(), packet.cbegin() + PACKET_HEADER_SIZE);
    //std::cout<<"processOnePacket - a" <<std::endl;
    uint16_t apid = pktReader.getAPID(header);
    //std::cout<<"processOnePacket - b apid "<<apid <<std::endl;
    uint16_t sourceSequenceCounter = pktReader.getSourceSequenceCounter(header);
    //std::cout<<"processOnePacket - c ssc "<<sourceSequenceCounter <<std::endl;
    uint16_t packetLength = pktReader.getPacketLength(header);
    //std::cout<<"processOnePacket - d pktlen "<<packetLength <<std::endl;

    auto payload = std::vector<uint8_t>(packet.cbegin() + PACKET_HEADER_SIZE, packet.cend());
    //std::cout<<"processOnePacket - e payload " <<std::endl;
    double timeStamp = pktReader.getPacketTimeStamp(payload);
    //std::cout<<"processOnePacket - f timeStamp "<<timeStamp <<std::endl;

    LogFileWriter::getInstance().logInfo("APID " + std::to_string(apid) + \
        " SSC " + std::to_string(sourceSequenceCounter) + \
        " pktLen " + std::to_string(packetLength) + \
        " time " + std::to_string(timeStamp) \
        );

    switch (apid) {
        case MEGSA_APID:
            //std::cout<<"processOnePacket - g calling processMegsAPacket " <<std::endl;
            processMegsAPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSB_APID:
            processMegsBPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case ESP_APID:
            processESPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case MEGSP_APID:
            processMegsPPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        case HK_APID:
            processHKPacket(payload, sourceSequenceCounter, packetLength, timeStamp);
            break;
        default:
            std::cerr << "Unrecognized APID: " << apid << std::endl;
            // Handle error or unknown APID case if necessary
            break;
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    //std::cout << "Elapsed time: " << elapsed_seconds.count() << " sec" << std::endl;
    uint64_t elapsedMicrosec = 1.e6 * elapsed_seconds.count();
    LogFileWriter::getInstance().logInfo("Elapsed microsec "+ std::to_string(elapsedMicrosec));
}

uint32_t payloadToTAITimeSeconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("Payload must contain at least 4 bytes.");
    }

    std::cout << "Payload TAI Seconds bytes in hex:" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "Byte " << i << ": 0x" 
                  << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(payload[i]) << std::dec << std::endl;
    }
    uint32_t tai = (static_cast<uint32_t>(payload[3]) << 24) |
           (static_cast<uint32_t>(payload[2]) << 16) |
           (static_cast<uint32_t>(payload[1]) << 8)  |
           static_cast<uint32_t>(payload[0]);
    uint32_t reversedtai = (static_cast<uint32_t>(payload[0]) << 24) |
           (static_cast<uint32_t>(payload[1]) << 16) |
           (static_cast<uint32_t>(payload[2]) << 8)  |
           static_cast<uint32_t>(payload[3]);

    std::cout << "payloadToTAITimeSeconds calculated " << tai << std::endl;
    std::cout << "payloadToTAITimeSeconds reversed calculated " << reversedtai << std::endl;
        // Print the combined 32-bit value in hex
    std::cout << "Combined 32-bit value from the TAI bytes: 0x" 
              << std::hex << std::setfill('0') << std::setw(8) 
              << tai << std::dec << std::endl;
    return tai;
}

uint32_t payloadToTAITimeSubseconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 6) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("Payload must contain at least 4 bytes.");
    }

    return (static_cast<uint32_t>(payload[4]) << 24) |
           (static_cast<uint32_t>(payload[5]) << 16);
}

// the payload starts with the secondary header timestamp
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, 
    double timeStamp) {

    //std::cout<<"processMegsAPacket a " <<std::endl;
    LogFileWriter::getInstance().logInfo("processMegsAPacket");

    // insert pixels into image

    uint16_t year, doy, hh, mm, ss;
    uint32_t sod;
    uint32_t tai_sec;
    static uint16_t processedPacketCounter=0;
    int8_t status=0;
    static bool testPattern;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC oneMEGSStructure;
    int vcdu_offset_to_sec_hdr = 20; // 6 bytes from packet start to timestamp, 8 byte IMPDU hdr, 6 byte CVCDU hdr is 20

    std::cout << "processMegsAPacket 1 " << oneMEGSStructure.vcdu_count << " im00"<<" "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";


    // payload starts at secondary header
    // 6-byte VCDU header, 8-byte IMPDU header, 6-byte primary packet header
    // data to copy starts at secondary header, includes 8-byte timestamp, 2 byte mode, then 2-byte pixel pairs (1752 bytes except for last packet)

    uint8_t vcdu[STANDARD_MEGSAB_PACKET_LENGTH + vcdu_offset_to_sec_hdr] = {0};

    // vcdu has 14 bytes before the packet start (vcdu=6, impdu=8)
    // pkthdr=6 bytes before the payload starts at the 2nd hdr timestamp
    // copy from payload into vcdu starting at byte 20 from the start of sync, 16 from start of VCDU
    std::copy(payload.begin(), payload.end(), vcdu + vcdu_offset_to_sec_hdr);

    // vcdu should look like a fake VCDU, with garbage before the timestamp
    // and it does, just without the primary header, impdu hdr, and vcdu hdr

    //VERIFIED THE COPY IS CORRECT, payload and vcdu are consistent
    //std::cout<<"vcdu modified"<<std::endl;
    //printBytesToStdOut(vcdu,0,48);
    //printBytesToStdOut(payload.data(),0,48);
    //exit(EXIT_FAILURE);

    //std::ostringstream oss;
    //oss << "MA SSC " << std::to_string(sourceSequenceCounter) << " pix";
    // //first 8 bytes of data (2 pixel pairs)
    //for (int i = 10; i <= 17; ++i) {
    //    oss << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(payload.data()[i]);
    //}
    //oss << " ...";
    // //last 8 bytes of data (2 pixel pairs)
    //size_t dataSize = payload.size();
    //for (size_t i = dataSize-8; i<dataSize; ++i) {
    //    oss << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(payload.data()[i]);
    //}

    //LogFileWriter::getInstance().logInfo(oss.str());


    if ((previousSrcSeqCount == -1) || (sourceSequenceCounter == 0) || 
        (sourceSequenceCounter <= previousSrcSeqCount)) {
        // packet is from a new image
        std::string logMsg = "MA starting new image first SrcSeqCounter: " + std::to_string(sourceSequenceCounter);
        LogFileWriter::getInstance().logInfo(logMsg);
        std::cout << "Info: " << logMsg << std::endl;

        //reset oneMEGSStructure
        oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 

        testPattern = false; // default is not a test pattern
        if ((vcdu[34] == 0) && (vcdu[35] == 2) && (vcdu[36] == 0) && (vcdu[37] == 1)) {
            testPattern = true; // use vcdu[34]==0 vcdu[35]=2 vcdu[36]=0 vcdu[37]=1 to identify TP (first 2 pixesl are bad so skip 30-33)
            std::cout << "processMegsAPacket identified a test pattern" <<std::endl;
        }

        // only assign the time from the first packet, the rest keep changing
        tai_sec = payloadToTAITimeSeconds(payload);
        oneMEGSStructure.tai_time_seconds = tai_sec;
        oneMEGSStructure.tai_time_subseconds = payloadToTAITimeSubseconds(payload);

        // assign current tai time to firstpkt_tai_time_seconds and subseconds
        TimeInfo currentTime;
        currentTime.updateNow();
        oneMEGSStructure.firstpkt_tai_time_seconds = currentTime.getTAISeconds();
        oneMEGSStructure.firstpkt_tai_time_subseconds = currentTime.getTAISubseconds();

        tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss);
        std::cout << "processMegsAPacket called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<" . "<< oneMEGSStructure.tai_time_subseconds/65535 <<"\n";

        processedPacketCounter=1;
    }
    previousSrcSeqCount = sourceSequenceCounter;

    // begin assigning data into oneMEGSStructure

    // assing pixel values from the packet into the proper locations in the image
    int parityErrors = assemble_image(vcdu, &oneMEGSStructure, sourceSequenceCounter, testPattern, &status);
    for (int row=0; row<8; row+=2) {
        std::cout << "processMegsAPacket R"<< row <<" vcdu_count " << oneMEGSStructure.vcdu_count << " im "<<oneMEGSStructure.image[0][row] <<" "<< oneMEGSStructure.image[1][row] << " "<<oneMEGSStructure.image[2][row] <<" "<<oneMEGSStructure.image[3][row] <<"\n";
    }
    for (int row=1023; row>1017; row-=2) {
        std::cout << "processMegsAPacket R"<< row <<" vcdu_count " << oneMEGSStructure.vcdu_count << " im "<<oneMEGSStructure.image[0][row] <<" "<< oneMEGSStructure.image[1][row] << " "<<oneMEGSStructure.image[2][row] <<" "<<oneMEGSStructure.image[3][row] <<"\n";
    }
    std::cout << "processMegsAPacket C0 0-3" <<" im "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";
    std::cout << "processMegsAPacket C0 510-513" <<" im "<<oneMEGSStructure.image[510][0] <<" "<< oneMEGSStructure.image[511][0] << " "<<oneMEGSStructure.image[512][0] <<" "<<oneMEGSStructure.image[512][0] <<"\n";


    //std::cout << "processMegsAPacket called assemble_image R0 vcdu_count " << oneMEGSStructure.vcdu_count << " im00"<<" "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";
    //std::cout << "processMegsAPacket called assemble_image R1 vcdu_count " << oneMEGSStructure.vcdu_count << " im01"<<" "<<oneMEGSStructure.image[0][1] <<" "<< oneMEGSStructure.image[1][1] << " "<<oneMEGSStructure.image[2][1] <<" "<<oneMEGSStructure.image[3][1] <<"\n";
    //std::cout << "processMegsAPacket called assemble_image R2 vcdu_count " << oneMEGSStructure.vcdu_count << " im02"<<" "<<oneMEGSStructure.image[0][2] <<" "<< oneMEGSStructure.image[1][2] << " "<<oneMEGSStructure.image[2][2] <<" "<<oneMEGSStructure.image[3][2] <<"\n";


    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MA parity errors: "+ std::to_string(parityErrors));
        std::cout << "processMegsAPacket - assemble_image returned parity errors: " << parityErrors << " ssc:"<<sourceSequenceCounter<<"\n";
    }

    if ( sourceSequenceCounter == 2394) {
        std::cout<<"end of MEGS-A at 2394"<<"\n";
        LogFileWriter::getInstance().logInfo("end of MEGS-A image ssc=2394");

        // may need to run this in another thread

        //std::cout<<"processMegsAPacket image 2047*1022"<<std::endl;
        //printUint16ToStdOut(oneMEGSStructure.image[2047*1022], MEGS_IMAGE_WIDTH, 10);


        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        std::cout << "processMegsAPacket number of packets: " << processedPacketCounter << std::endl;
        if (fitsFileWriter) {
            std::cout << "processMegsAPacket: tai_time_seconds = " << oneMEGSStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeMegsAFITS( oneMEGSStructure )) {
                LogFileWriter::getInstance().logInfo("writeMegsAFITS write error");
                std::cout << "ERROR: writeMegsAFITS returned an error" << std::endl;
            }
            // reset the structure immediately after writing
            oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 
        }
    }
    processedPacketCounter++; // count packets processed

}

void processMegsBPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}
void processMegsPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}
void processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
}

void processHKPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {
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