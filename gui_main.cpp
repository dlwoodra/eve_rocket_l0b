/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
*  target is XEM7310-a75
*/

#include "commonFunctions.hpp"
#include "eve_l0b.hpp"
#include <csignal> // needed for SIGINT

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
//#if defined(IMGUI_IMPL_OPENGL_ES2)
//#include <GLES2/gl2.h>
//#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "implot.h"  // Include ImPlot if you are using it
#include <vector>
#include <algorithm> // for std::clamp
#include <utility> // for std::pair

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

uint16_t CCD_image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH];  // Original image data

// Function to find the brightest pixel
std::pair<int, int> FindBrightestPixel() {
    uint16_t max_value = 0;
    int max_x = 0, max_y = 0;
    int img_height = MEGS_IMAGE_HEIGHT;
    int img_width = MEGS_IMAGE_WIDTH;
    
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            if (CCD_image[y][x] > max_value) {
                max_value = CCD_image[y][x];
                max_x = x;
                max_y = y;
            }
        }
    }
    return {max_x, max_y};  // Return coordinates of the brightest pixel
}

// Function to calculate centroid for a 9x9 region around the brightest pixel
std::pair<float, float> CalculateCentroid(int brightest_x, int brightest_y) {
    int sum_x = 0, sum_y = 0, sum_value = 0;
    int img_height = MEGS_IMAGE_HEIGHT;
    int img_width = MEGS_IMAGE_WIDTH;

    for (int y = -4; y <= 4; ++y) {
        for (int x = -4; x <= 4; ++x) {
            int ny = brightest_y + y;
            int nx = brightest_x + x;
            if (nx >= 0 && nx < img_width && ny >= 0 && ny < img_height) {
                uint16_t pixel_value = CCD_image[ny][nx];
                sum_x += nx * pixel_value;
                sum_y += ny * pixel_value;
                sum_value += pixel_value;
            }
        }
    }

    if (sum_value == 0) {
        return {brightest_x, brightest_y};  // Return the brightest pixel if the sum is 0
    }
    
    // Compute centroid coordinates
    float centroid_x = sum_x / static_cast<float>(sum_value);
    float centroid_y = sum_y / static_cast<float>(sum_value);
    
    return {centroid_x, centroid_y};
}

// Function to plot rows and columns, and display centroid information
void PlotData() {
    int img_height = MEGS_IMAGE_HEIGHT;
    int img_width = MEGS_IMAGE_WIDTH;
    // Plot rows
    if (ImPlot::BeginPlot("Rows")) {
        for (int y = 0; y < img_height; ++y) {
            ImPlot::PlotLine("Row", &CCD_image[y][0], img_width);
        }
        ImPlot::EndPlot();
    }

    // Plot columns stacked
    if (ImPlot::BeginPlot("Columns")) {
        std::vector<uint16_t> column_data(img_height);
        for (int x = 0; x < img_width; ++x) {
            for (int y = 0; y < img_height; ++y) {
                column_data[y] = CCD_image[y][x];
            }
            ImPlot::PlotLine("Column", column_data.data(), img_height);
        }
        ImPlot::EndPlot();
    }

    // Find brightest pixel (replacing structured bindings with std::pair)
    std::pair<int, int> brightest_pixel = FindBrightestPixel();
    int brightest_x = brightest_pixel.first;
    int brightest_y = brightest_pixel.second;

    // Calculate centroid (replacing structured bindings with std::pair)
    std::pair<float, float> centroid = CalculateCentroid(brightest_x, brightest_y);
    float centroid_x = centroid.first;
    float centroid_y = centroid.second;


    // Display results
    ImGui::Text("Brightest Pixel: (%d, %d)", brightest_x, brightest_y);
    ImGui::Text("Centroid: (%.2f, %.2f)", centroid_x, centroid_y);
}

// prototypes
void print_help();
void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord);
void extern processPackets_withgui(CCSDSReader& pktReader, std::unique_ptr<RecordFileWriter>& recordWriter, bool skipRecord, GLFWwindow* window);
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

// Function to handle the Ctrl-C (SIGINT) signal
void handleSigint(int signal) {
    std::cout << "\nCaught Ctrl-C (SIGINT)! Cleaning up and exiting..." << std::endl;

    spdlog::info("SIGINT received, flushing log and exiting.");
    spdlog::shutdown();

    std::exit(signal); // Exit the program with the signal code
}

int main(int argc, char* argv[]) {
    std::string filename;
    bool skipESP = false;
    bool skipMP = false;
    bool skipRecord = false;

    // Register the signal handler for SIGINT
    std::signal(SIGINT, handleSigint);

    std::cout << "Program running. Press Ctrl-C to exit." << std::endl;
    
    parseCommandLineArgs(argc, argv, filename, skipESP, skipMP, skipRecord);

    // initialize ImGui and ImPlot
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
   // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // glfw is setup

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
            processPackets_withgui(fileReader, recordWriter, skipRecord, window);
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

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}

void parseCommandLineArgs(int argc, char* argv[], std::string& filename, bool& skipESP, bool& skipMP, bool& skipRecord) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (isValidFilename(arg) && arg[0] != '-') {
            filename = arg;
            LogFileWriter::getInstance().logInfo("Received filename arg: " + arg);
        } else if (arg == "--skipESP" || arg == "-skipESP") {
            skipESP = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
        } else if (arg == "--skipMP" || arg == "-skipMP") {
            skipMP = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
        } else if (arg == "--help" || arg == "-help") {
            print_help();
        } else if (arg == "--skipRecord" || arg == "-skipRecord") {
            skipRecord = true;
            LogFileWriter::getInstance().logInfo("Received : " + arg);
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