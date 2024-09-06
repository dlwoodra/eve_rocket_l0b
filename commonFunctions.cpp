// commonFunctions.cpp

#include "commonFunctions.hpp"
//#include "eve_l0b.hpp" // included in commonFunctions.hpp


// Check if the compiler supports __builtin_bswap32
#if defined(__has_builtin)
    #if __has_builtin(__builtin_bswap32)
        #define HAS_BUILTIN_BSWAP32 1
    #endif
#elif defined(__GNUC__)
    #if (__GNUC__ >= 4) // __builtin_bswap32 is supported since GCC 4.x
        #define HAS_BUILTIN_BSWAP32 1
    #endif
#endif


// Define a byte-swapping function
inline uint32_t byteswap_32(uint32_t value) {
#if defined(HAS_BUILTIN_BSWAP32)
    return __builtin_bswap32(value);
#else
    // Fallback: manual byte swap using bit shifts
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
#endif
}

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
        std::cout << "processPackets counter " << counter++ << std::endl;

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

    return (static_cast<uint32_t>(payload[0]) << 24) |
           (static_cast<uint32_t>(payload[1]) << 16) |
           (static_cast<uint32_t>(payload[2]) << 8)  |
           static_cast<uint32_t>(payload[3]);
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
    int8_t status=0;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC megsStructure;
    //const MEGS_IMAGE_REC megsStructureInit;
    //struct MEGS_IMAGE_REC megsBStructure;
    int vcdu_impdu_prihdr_length = 20;
    uint8_t pktarr[STANDARD_MEGSAB_PACKET_LENGTH + vcdu_impdu_prihdr_length];

    //std::cout<<"processMegsAPacket b " <<std::endl;

    // vcdu has 14 bytes before the packet start (vcdu=6, impdu=8)
    // pkthdr=6 bytes before the payload starts at the 2nd hdr timestamp
    // copy from payload into pktarr starting at byte 20
    std::copy(payload.begin(), payload.end(), pktarr + vcdu_impdu_prihdr_length);

    //std::cout<<"processMegsAPacket c " <<std::endl;

    tai_sec = payloadToTAITimeSeconds(payload);
    megsStructure.tai_time_seconds = tai_sec;
    megsStructure.tai_time_subseconds = payloadToTAITimeSubseconds(payload);

    tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss);
    std::cout << "called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<" . "<< megsStructure.tai_time_subseconds/65535 <<"\n";

    if (previousSrcSeqCount + 1 == sourceSequenceCounter) {
        if (sourceSequenceCounter != 0) {
            // continuing the same image
        } else {
            // packet is from a new image
            std::string logMsg = "MA starting new image first SrcSeqCounter: " + std::to_string(sourceSequenceCounter);
            LogFileWriter::getInstance().logInfo(logMsg);
            std::cout << "Info: " << logMsg << std::endl;
            //TODO: reset packet counter
            //megsStructure = megsStructureInit;
        }
        previousSrcSeqCount = sourceSequenceCounter;
    }

    int parityErrors = assemble_image(pktarr, &megsStructure, sourceSequenceCounter, &status);
    //std::cout << "called assemble_image" << "\n";


    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MA parity errors: "+ std::to_string(parityErrors));
        std::cout << "assemble_image returned parity errors: " << parityErrors << "\n";
    }

    if ( sourceSequenceCounter == 2394) {
        std::cout<<"end of MEGS-A at 2394"<<"\n";
        LogFileWriter::getInstance().logInfo("end of MEGS-A image ssc=2394");

        // may need to run this in another thread

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        if (fitsFileWriter) {
            if (!fitsFileWriter->writeMegsAFITS( megsStructure )) {

            }
        //  if (!fitsFileWriter->writePacketToFITS(packet, apid, timeStamp)) {
        //    std::cerr << "ERROR: Failed to write packet to FITS file." << std::endl;
        //    return;
        //  }
        }
    }

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