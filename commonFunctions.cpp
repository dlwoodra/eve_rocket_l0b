// commonFunctions.cpp

#include "commonFunctions.hpp"

#ifdef ENABLEGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
//#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "implot.h"  // Include ImPlot if you are using it

// reads a packet and writes it to the recordfile
void processPackets_withgui(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord, GLFWwindow* window) {
    std::vector<uint8_t> packet;
    int counter = 0;

    while (!glfwWindowShouldClose(window)) {
        if (pktReader.readNextPacket(packet)) {
            counter++;
            if (!skipRecord && recordWriter && !recordWriter->writeSyncAndPacketToRecordFile(packet)) {
                LogFileWriter::getInstance().logError("ERROR: processPackets failed to write packet to record file.");
                return;
            }
            // Process packet
            processOnePacket(pktReader, packet);

            // update plotcounter

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //Sleep(100);
        }
    }
}
#endif

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
        throw std::invalid_argument("payloadToTAITimeSeconds - Payload must contain at least 4 bytes.");
    }

    //std::cout << "Payload TAI Seconds bytes in hex:" << std::endl;
    //for (int i = 0; i < 4; ++i) {
    //    std::cout << "Byte " << i << ": 0x" 
    //              << std::hex << std::setfill('0') << std::setw(2) 
    //              << static_cast<int>(payload[i]) << std::dec << std::endl;
    //}

    //uint32_t tai = (static_cast<uint32_t>(payload[3]) << 24) |
    //       (static_cast<uint32_t>(payload[2]) << 16) |
    //       (static_cast<uint32_t>(payload[1]) << 8)  |
    //       static_cast<uint32_t>(payload[0]);
    uint32_t reversedtai = (static_cast<uint32_t>(payload[0]) << 24) |
           (static_cast<uint32_t>(payload[1]) << 16) |
           (static_cast<uint32_t>(payload[2]) << 8)  |
           static_cast<uint32_t>(payload[3]);

    std::cout << "payloadToTAITimeSeconds reversed calculated " << reversedtai << std::endl;
        // Print the combined 32-bit value in hex
    //std::cout << "payloadToTAITimeSeconds TAI bytes: 0x" 
    //          << std::hex << std::setfill('0') << std::setw(8) 
    //          << tai << std::dec << std::endl;
    return reversedtai;
}

uint32_t payloadToTAITimeSubseconds(const std::vector<uint8_t>& payload) {
    if (payload.size() < 6) {
        // Handle the error case, perhaps by throwing an exception
        throw std::invalid_argument("payloadToTAITimeSubseconds - Payload must contain at least 4 bytes.");
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
    std::cout << "populateStructureTimes called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<" . "<< oneStructure.tai_time_subseconds / 65535 << "\n";
    
    oneStructure.sod = (uint32_t)sod;
    oneStructure.yyyydoy = (uint32_t)(year * 1000 + doy);
    oneStructure.iso8601 = iso8601;
    
    std::cout << "structure iso time " << iso8601 << std::endl;
}

// the payload starts with the secondary header timestamp
void processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    LogFileWriter::getInstance().logInfo("processMegsAPacket");

    // insert pixels into image

    uint16_t year, doy, hh, mm, ss;
    uint32_t sod;
    uint32_t tai_sec;
    static std::string iso8601;

    static uint16_t processedPacketCounter=0;
    int8_t status=0;
    static bool testPattern;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC oneMEGSStructure;
    int vcdu_offset_to_sec_hdr = 20; // 6 bytes from packet start to timestamp, 8 byte IMPDU hdr, 6 byte CVCDU hdr is 20

    //std::cout << "processMegsAPacket 1 " << oneMEGSStructure.vcdu_count << " im00"<<" "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";


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
        oneMEGSStructure.rec_tai_seconds = currentTime.getTAISeconds();
        oneMEGSStructure.rec_tai_subseconds = currentTime.getTAISubseconds();

        tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss, iso8601);
        std::cout << "processMegsAPacket called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<" . "<< oneMEGSStructure.tai_time_subseconds/65535 <<"\n";
        oneMEGSStructure.sod = (uint32_t) sod;
        oneMEGSStructure.yyyydoy = (uint32_t) year*1000 + doy;
        oneMEGSStructure.iso8601 = iso8601;
        std::cout<<"writeMegsAFITS iso "<<iso8601<<std::endl;

        processedPacketCounter=0;
    }
    previousSrcSeqCount = sourceSequenceCounter;

    // begin assigning data into oneMEGSStructure

    // assing pixel values from the packet into the proper locations in the image
    int parityErrors = assemble_image(vcdu, &oneMEGSStructure, sourceSequenceCounter, testPattern, &status);

    // for (int row=0; row<8; row+=2) {
    //     std::cout << "processMegsAPacket R"<< row <<" vcdu_count " << oneMEGSStructure.vcdu_count << " im "<<oneMEGSStructure.image[0][row] <<" "<< oneMEGSStructure.image[1][row] << " "<<oneMEGSStructure.image[2][row] <<" "<<oneMEGSStructure.image[3][row] <<"\n";
    // }
    // for (int row=1023; row>1017; row-=2) {
    //     std::cout << "processMegsAPacket R"<< row <<" vcdu_count " << oneMEGSStructure.vcdu_count << " im "<<oneMEGSStructure.image[0][row] <<" "<< oneMEGSStructure.image[1][row] << " "<<oneMEGSStructure.image[2][row] <<" "<<oneMEGSStructure.image[3][row] <<"\n";
    // }
    // std::cout << "processMegsAPacket C0 0-3" <<" im "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";
    // std::cout << "processMegsAPacket C0 510-513" <<" im "<<oneMEGSStructure.image[510][0] <<" "<< oneMEGSStructure.image[511][0] << " "<<oneMEGSStructure.image[512][0] <<" "<<oneMEGSStructure.image[512][0] <<"\n";

    //std::cout << "processMegsAPacket called assemble_image R0 vcdu_count " << oneMEGSStructure.vcdu_count << " im00"<<" "<<oneMEGSStructure.image[0][0] <<" "<< oneMEGSStructure.image[1][0] << " "<<oneMEGSStructure.image[2][0] <<" "<<oneMEGSStructure.image[3][0] <<"\n";
    //std::cout << "processMegsAPacket called assemble_image R1 vcdu_count " << oneMEGSStructure.vcdu_count << " im01"<<" "<<oneMEGSStructure.image[0][1] <<" "<< oneMEGSStructure.image[1][1] << " "<<oneMEGSStructure.image[2][1] <<" "<<oneMEGSStructure.image[3][1] <<"\n";
    //std::cout << "processMegsAPacket called assemble_image R2 vcdu_count " << oneMEGSStructure.vcdu_count << " im02"<<" "<<oneMEGSStructure.image[0][2] <<" "<< oneMEGSStructure.image[1][2] << " "<<oneMEGSStructure.image[2][2] <<" "<<oneMEGSStructure.image[3][2] <<"\n";

    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MA parity errors: "+ std::to_string(parityErrors));
        std::cout << "processMegsAPacket - assemble_image returned parity errors: " << parityErrors << " ssc:"<<sourceSequenceCounter<<"\n";
    }
    processedPacketCounter++; // count packets processed

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
            processedPacketCounter=0;
            // reset the structure immediately after writing
            oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 
        }
    }
}

void processMegsBPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    LogFileWriter::getInstance().logInfo("processMegsBPacket");

    // insert pixels into image

    uint16_t year, doy, hh, mm, ss;
    uint32_t sod;
    uint32_t tai_sec;
    static std::string iso8601;

    static uint16_t processedPacketCounter=0;
    int8_t status=0;
    static bool testPattern;
    static int32_t previousSrcSeqCount = -1;
    static MEGS_IMAGE_REC oneMEGSStructure;
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
        std::string logMsg = "MB starting new image first SrcSeqCounter: " + std::to_string(sourceSequenceCounter);
        LogFileWriter::getInstance().logInfo(logMsg);
        std::cout << "Info: " << logMsg << std::endl;

        //reset oneMEGSStructure
        oneMEGSStructure = MEGS_IMAGE_REC{0}; // c++11 

        testPattern = false; // default is not a test pattern
        //if ((vcdu[34] == 0) && (vcdu[35] == 2) && (vcdu[36] == 0) && (vcdu[37] == 1)) { //NEED TO CHANGE FOR MEGS-B
            testPattern = true; // use vcdu[34]==0 vcdu[35]=2 vcdu[36]=0 vcdu[37]=1 to identify TP (first 2 pixesl are bad so skip 30-33)
            std::cout << "processMegsBPacket identified a test pattern" <<std::endl;
        //}

        // only assign the time from the first packet, the rest keep changing
        tai_sec = payloadToTAITimeSeconds(payload);
        oneMEGSStructure.tai_time_seconds = tai_sec;
        oneMEGSStructure.tai_time_subseconds = payloadToTAITimeSubseconds(payload);

        // assign current tai time to firstpkt_tai_time_seconds and subseconds
        TimeInfo currentTime;
        currentTime.updateNow();
        oneMEGSStructure.rec_tai_seconds = currentTime.getTAISeconds();
        oneMEGSStructure.rec_tai_subseconds = currentTime.getTAISubseconds();

        tai_to_ydhms(tai_sec, &year, &doy, &sod, &hh, &mm, &ss, iso8601);
        std::cout << "processMegsBPacket called tai_to_ydhms " << year << " "<< doy << "-" << hh << ":" << mm << ":" << ss <<" . "<< oneMEGSStructure.tai_time_subseconds/65535 <<"\n";
        oneMEGSStructure.sod = (uint32_t) sod;
        oneMEGSStructure.yyyydoy = (uint32_t) year*1000 + doy;
        oneMEGSStructure.iso8601 = iso8601;
        std::cout<<"writeMegsBFITS iso "<<iso8601<<std::endl;

        processedPacketCounter=0;
    }
    previousSrcSeqCount = sourceSequenceCounter;

    // begin assigning data into oneMEGSStructure

    // assing pixel values from the packet into the proper locations in the image
    int parityErrors = assemble_image(vcdu, &oneMEGSStructure, sourceSequenceCounter, testPattern, &status);

    if ( parityErrors > 0 ) {
        LogFileWriter::getInstance().logError("MB parity errors: "+ std::to_string(parityErrors));
        std::cout << "processMegsBPacket - assemble_image returned parity errors: " << parityErrors << " ssc:"<<sourceSequenceCounter<<"\n";
    }

    processedPacketCounter++; // count packets processed

    if ( sourceSequenceCounter == 2394) {
        std::cout<<"end of MEGS-B at 2394"<<"\n";
        LogFileWriter::getInstance().logInfo("end of MEGS-B image ssc=2394");

        // may need to run this in another thread

        // Write packet data to a FITS file if applicable
        std::unique_ptr<FITSWriter> fitsFileWriter;
        fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
        // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();
        std::cout << "processMegsBPacket number of packets: " << processedPacketCounter << std::endl;
        if (fitsFileWriter) {
            std::cout << "procesMegsBPacket: tai_time_seconds = " << oneMEGSStructure.tai_time_seconds << std::endl;

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
    static uint16_t processedPacketCounter=0;

    //std::cout<<"processMegsPPacket 1" << std::endl;

    populateStructureTimes(oneMEGSPStructure, payload);
    //std::cout<<"processMegsPPacket 2" << std::endl;

    int firstbyteoffset = 10;

    int packetoffset = processedPacketCounter * MEGSP_PACKETS_PER_FILE;
    //std::cout<<"processMegsPPacket packetoffset "<< packetoffset << std::endl;
    for (int i=0; i<MEGSP_INTEGRATIONS_PER_PACKET; ++i) {
        int incr = (i*4) + firstbyteoffset; // 4 is bytes per integration, 2 bytes per diode * 2 diodes per integration
        int index = packetoffset + i;
        oneMEGSPStructure.MP_lya[index] = (uint16_t (payload[incr]) << 8) | (uint16_t (payload[incr + 1]));
        oneMEGSPStructure.MP_dark[index] = (uint16_t (payload[incr+2]) << 8) | (uint16_t (payload[incr+3]));
    }

    processedPacketCounter++;
    //std::cout<<"processMegsPPacket processedPacketCounter "<< processedPacketCounter << std::endl;

    // Write packet data to a FITS file if applicable
    std::unique_ptr<FITSWriter> fitsFileWriter;
    fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
    // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();

    // ONLY WRITE WHEN STRUCTURE IS FULL
    if ( processedPacketCounter == MEGSP_PACKETS_PER_FILE ) {
        if (fitsFileWriter) {
            std::cout << "procesMegsPPacket: tai_time_seconds = " << oneMEGSPStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeMegsPFITS( oneMEGSPStructure )) {
                LogFileWriter::getInstance().logInfo("writeMegsPFITS write error");
                std::cout << "ERROR: writeMegsPFITS returned an error" << std::endl;
            }
            std::cout<<"processMegsPPacket - MP_lya values" << std::endl;
            printBytes(oneMEGSPStructure.MP_lya,19);

            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneMEGSPStructure = MEGSP_PACKET{0}; // c++11 
        }
    }
}

void processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp) {

    ESP_PACKET oneESPStructure = {0};
    static uint16_t processedPacketCounter=0;

    populateStructureTimes(oneESPStructure, payload);

    int firstbyteoffset = 10;

    int packetoffset = processedPacketCounter * ESP_PACKETS_PER_FILE;
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
    }

    processedPacketCounter++;
    //std::cout<<"processMegsPPacket processedPacketCounter "<< processedPacketCounter << std::endl;

    // Write packet data to a FITS file if applicable
    std::unique_ptr<FITSWriter> fitsFileWriter;
    fitsFileWriter = std::unique_ptr<FITSWriter>(new FITSWriter());
    // the c++14 way fitsFileWriter = std::make_unique<FITSWriter>();

    // ONLY WRITE WHEN STRUCTURE IS FULL
    if ( processedPacketCounter == ESP_PACKETS_PER_FILE ) {
        if (fitsFileWriter) {
            std::cout << "procesESPPacket: tai_time_seconds = " << oneESPStructure.tai_time_seconds << std::endl;

            if (!fitsFileWriter->writeESPFITS( oneESPStructure )) {
                LogFileWriter::getInstance().logInfo("writeESPFITS write error");
                std::cout << "ERROR: writESPFITS returned an error" << std::endl;
            }
            std::cout<<"processESPPacket - ESP diode 0 values" << std::endl;
            printBytes(oneESPStructure.ESP_q0,19);

            processedPacketCounter = 0;
            // reset the structure immediately after writing
            oneESPStructure = ESP_PACKET{0}; // c++11 
        }
    }

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