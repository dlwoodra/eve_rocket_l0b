/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
*  target is XEM7310-a75.
*    
*  Functionality expanded to extract content from packets, assemble images,
*  aggregate multiple packets together for writing to FITS files as
*  level 0b data. 
*
*  Additional functionality to display some data using imgui with implot is intended.
*
*/

#include "commonFunctions.hpp"
#include "eve_l0b.hpp"

#include <csignal> // needed for SIGINT


// prototypes
void print_help();
void globalStateInit();
void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord);
void extern processPackets(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord);
void extern processOnePacket(CCSDSReader& pktReader, const std::vector<uint8_t>& packet);
void extern processMegsAPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void extern processMegsBPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void extern processMegsPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void extern processESPPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);
void extern processHKPacket(std::vector<uint8_t> payload, 
    uint16_t sourceSequenceCounter, uint16_t packetLength, double timeStamp);

std::mutex mtx;
ProgramState globalState;
#ifdef ENABLEGUI
int imgui_thread();
// Start the ImGui loop in a new thread object imguiThread
// passing function pointer imgui_thread
std::thread imguiThread(imgui_thread);
#endif


// Function to handle the Ctrl-C (SIGINT) signal
void handleSigint(int signal) {
    std::cout << "\nCaught Ctrl-C (SIGINT)! Cleaning up and exiting..." << std::endl;

    spdlog::info("SIGINT received, flushing log and exiting.");
    spdlog::shutdown();

    // in order to flush the recordFileWriter a global variable is needed
    // I'd rather keep this simple
    // we could do by making a global
    //like RecordFileWriter* g_recordFileWriter = nullptr;
    // then in here there would be code
    // like         
    // Flush the buffer and close the file
    //    if (g_recordFileWriter) {
    //        g_recordFileWriter->flush();
    //        g_recordFileWriter->close();
    //    }

    // then in main this would be needed
    //// Assign the global pointer
    //g_recordFileWriter = &recordFileWriter;

    globalState.running = false;  // allow dear imgui to shut itself down
    // wait a short amount for imgui to respond
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // instead join the imgui thread
#ifdef ENABLEGUI
    imguiThread.join();
#endif
    std::exit(signal); // Exit the program with the signal code
}

int main(int argc, char* argv[]) {
    std::string filename;
    bool skipESP = false;
    bool skipMP = false;
    bool skipRecord = false;

    // initialize the programState structure contents
    globalStateInit();

    // Register the signal handler for SIGINT
    std::signal(SIGINT, handleSigint);

    std::cout << "Program running. Press Ctrl-C to exit." << std::endl;

    static const char* env_eve_data_root = std::getenv("eve_data_root");
    if (env_eve_data_root == nullptr) {
        std::cout<< "***";
        std::cout << "ERROR: environment variable eve_data_root is undefined - aborting" <<std::endl;
        std::cout << " Run the appropriate setup script:" <<std::endl;
        std::cout << "  in tcsh, source setup_eve_rdp.csh" <<std::endl;
        std::cout << "  in bash, . setup_eve_rdp.csh" <<std::endl;
        std::cout<< "***";
        handleSigint(SIGINT); // call the signal handler to clean up and exit
        exit(EXIT_FAILURE);
    }

    parseCommandLineArgs(argc, argv, filename, skipESP, skipMP, skipRecord);

    std::unique_ptr<RecordFileWriter> recordWriter;
    if (!skipRecord) {
        recordWriter = std::unique_ptr<RecordFileWriter>(new RecordFileWriter());
        // the c++14 way recordWriter = std::make_unique<RecordFileWriter>();
    }

    if (isValidFilename(filename)) {
        // read packets from the file provided in the argument list
        FileInputSource fileSource(filename);
        CCSDSReader fileReader(&fileSource);

        if (fileReader.open()) {
            processPackets(fileReader, recordWriter, skipRecord);
        } else {
            std::cerr << "Failed to open file argument "<< filename << std::endl;
            return EXIT_FAILURE;  
        }
        fileReader.close();

    } else {
        // read packets from USB

        // THIS IS JUST A STUB
        std::string serialNumber; 
        USBInputSource usbSource(serialNumber);

        // create a CCSDSReader instance        
        CCSDSReader usbReader(&usbSource);
        std::cout << "main: Created CCSDSReader usbReader object."  << std::endl;

        //pass usbReader by reference
        usbSource.CGProcRx(usbReader); // receive, does not return until disconnect
        //usbSource.replaceCGProxRx(usbReader); // receive, does not return until disconnect
        usbReader.close();

    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::cout << "Finished at " << std::ctime(&end_time) << std::endl;

    handleSigint(SIGINT); // call the signal handler to clean up and exit
    return EXIT_SUCCESS;
}

void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (isValidFilename(arg) && arg[0] != '-') {
            filename = arg;
            LogFileWriter::getInstance().logInfo("Received filename arg: {}", arg);
        } else if (arg == "--skipESP" || arg == "-skipESP") {
            skipESP = true;
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--skipMP" || arg == "-skipMP") {
            skipMP = true;
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--help" || arg == "-help") {
            print_help();
        } else if (arg == "--skipRecord" || arg == "-skipRecord") {
            skipRecord = true;
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else {
            LogFileWriter::getInstance().logError("Unknown command line option: " + arg);
            std::cerr << "Unknown option: " << arg << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

void globalStateInit() {
    std::cout << "globablStateInit executing" << std::endl;
    LogFileWriter::getInstance().logInfo("globalStateInit executing");
    //std::lock_guard<std::mutex> lock(mtx); // lock the mutex
    mtx.lock();
    globalState.megsa.image[0][0] = {0xff};
    globalState.megsb.image[0][0] = {0x3fff};
    globalState.running = true;
#ifdef ENABLEGUI
    globalState.guiEnabled = true;
#endif
    mtx.unlock(); // unlock the mutex
}

void print_help() {
  std::cout << "Main c++ program to support SDO EVE Rocket calibration at SURF." << std::endl;
  std::cout << "Compiled: " << __DATE__ << " " << __TIME__ << std::endl;
  std::cout << "Compiler: " << __VERSION__ << std::endl;
  std::cout << "Basefile: " << __BASE_FILE__ << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << " ./ql [tlmfilename] [options]" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Options: " << std::endl;
  std::cout << " -help runs print_help to display this message and exit" << std::endl;
  std::cout << " -skipESP will ignore ESP packets (apid 605)" << std::endl;
  std::cout << " -skipMP will ignore MEGS-P packets (apid 604)" << std::endl;
  std::cout << " -skipRecord disable recording of telemetry to a file" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "When provided, tlmfilename is a binary file of sync_marker,packet pairs. " << std::endl;
  std::cout << "Recorded files can be played back this way. " << std::endl;
  std::cout << "When using tlmfilename, -skipRecord will avoid creating additional files." << std::endl;
  std::cout << "The -skipRecord option should not be used for opalkelly connections." << std::endl;
  exit(EXIT_FAILURE);
}