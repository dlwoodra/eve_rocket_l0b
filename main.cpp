/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file.
*  target is XEM7310-a75.
*    
*  Functionality expanded to extract content from packets, assemble images,
*  aggregate multiple packets together for writing to FITS files as
*  level 0b data. SHK conversions are also performed.
*
*  Additional functionality to display some data using imgui with implot is intended.
*
*/

#include "commonFunctions.hpp"
#include "eve_l0b.hpp"
#include "ProgramState.hpp"
#include "FileCompressor.hpp"

#include <csignal> // needed for SIGINT
#include <optional>

// prototypes
void print_help();
void globalStateInit();
void parseCommandLineArgs(int argc, char* argv[]);
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

// global variables
std::optional<std::thread> imguiThread;

#ifdef ENABLEGUI
int imgui_thread();
#endif

// Function to handle the Ctrl-C (SIGINT) signal
void handleSigint(int signal) {
    std::cout << "\nCaught Ctrl-C (SIGINT)! Cleaning up and exiting..." << std::endl;

    // imgui and filecompressor threads self-terminate when globalState.running is set to false
    globalState.running.store(false);  // allow dear imgui to shut itself down

    // join the imgui thread if it is still joinable
    if (imguiThread && imguiThread->joinable()) {
        imguiThread->join();
    }

    // other threads may write to the log, so close the log last
    LogFileWriter::getInstance().logInfo("SIGINT received, flushing log and exiting.");
    // Close the log file and compress it
    LogFileWriter::getInstance().close();

    std::cout <<  "Log file closed and compressed." << std::endl;
    exit(EXIT_SUCCESS);
    return;
    
}

int main(int argc, char* argv[]) {

    // initialize the programState structure contents
    globalStateInit();

#ifdef ENABLEGUI
    imguiThread.emplace(imgui_thread); // Start the GUI thread safely after initialization
#endif

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

    parseCommandLineArgs(argc, argv);

    bool skipRecord = globalState.args.skipRecord.load();

    std::unique_ptr<RecordFileWriter> recordWriter;
    if (!skipRecord) {
        recordWriter = std::unique_ptr<RecordFileWriter>(new RecordFileWriter());
        // the c++14 way recordWriter = std::make_unique<RecordFileWriter>();
    }

    std::string filename; // input filename specified on command line
    if ( globalState.args.fileSpecified.load()) {
        mtx.lock();
        filename = globalState.args.filename;
        mtx.unlock();
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
        usbReader.close();

    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::cout << "Finished at " << std::ctime(&end_time) << std::endl;

    handleSigint(SIGINT); // call the signal handler to clean up and exit

    std::cout <<  "Clean termination" << std::endl;
    return EXIT_SUCCESS;
}

void parseCommandLineArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (isValidFilename(arg) && arg[0] != '-') {
            globalState.args.fileSpecified.store(true);
            globalState.args.filename = arg;
            LogFileWriter::getInstance().logInfo("Received filename arg: {}", arg);
        } else if (arg == "--skipESP" || arg == "-skipESP") {
            globalState.args.skipESP.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--skipMP" || arg == "-skipMP") {
            globalState.args.skipMP.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--writeBinaryRxBuff" || arg == "-writeBinaryRxBuff") {
            globalState.args.writeBinaryRxBuff.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--readBinAsUSB" || arg == "-readBinAsUSB") {
            globalState.args.readBinAsUSB.store(true);
            globalState.args.skipRecord.store(true);
            globalState.args.slowReplay.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--slowReplay" || arg == "-slowReplay") {
            globalState.args.slowReplay.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else if (arg == "--help" || arg == "-help") {
            print_help();
        } else if (arg == "--skipRecord" || arg == "-skipRecord") {
            globalState.args.skipRecord.store(true);
            LogFileWriter::getInstance().logInfo("Received : {}", arg);
        } else {
            LogFileWriter::getInstance().logError("Unknown command line option: " + arg);
            std::cerr << "Unknown option: " << arg << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

void print_help() {
  std::cout << "Main c++ program to support SDO EVE Rocket calibration at SURF." << std::endl;
  std::cout << "Compiled: " << __DATE__ << " " << __TIME__ << std::endl;
  std::cout << "Compiler: " << __VERSION__ << std::endl;
  std::cout << "Basefile: " << __BASE_FILE__ << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << " ./rl0b_main_gui [tlmfilename] [options]" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "Options: " << std::endl;
  std::cout << " -help runs print_help to display this message and exit" << std::endl;
  std::cout << " -skipESP will ignore ESP packets (apid 605)" << std::endl;
  std::cout << " -skipMP will ignore MEGS-P packets (apid 604)" << std::endl;
  std::cout << " -skipRecord disable recording of telemetry to a file" << std::endl;
  std::cout << " -slowReplay adds a sleep to slow down the processing" << std::endl;
  std::cout << " -writeBinaryRxBuff writes the large binary file of each 64k FIFO read" << std::endl;
  std::cout << " " << std::endl;
  std::cout << "When provided, tlmfilename is a binary file of sync_marker,packet pairs. " << std::endl;
  std::cout << "Recorded files can be played back this way. " << std::endl;
  std::cout << "When using tlmfilename, -skipRecord will avoid creating additional files." << std::endl;
  std::cout << "The -skipRecord option should not be used for OpalKelly connections." << std::endl;
  exit(EXIT_FAILURE);
}