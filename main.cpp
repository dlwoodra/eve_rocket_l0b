/*
*  Name: main.cpp
*  Description: This is the implementation for the minimum processing
*  capability. Open a connection to the USB via an opal kelly api,
*  read packets, write packets to a file separated by a 32-bit timestamp.
*
*  Usage: 
*/
#include <iostream>
#include <string>
#include <cstring> //for strlen
#include <chrono>
#include <ctime>

/* include opal kelly fronpanel */
//#include "okFrontPanelDLL.h"
// recall <> search only the standard directory
// double quotes search current directory first, then the standard directory

// *********************************************************************/
// Settings
// *********************************************************************/
#define MAX_DEAD_TIME_MS     200

// *********************************************************************/
// Globals
// *********************************************************************/

// pointer to front panel object
//OpalKelly::FrontPanelDevices devices;
//OpalKelly::FrontPanelPtr devptr;
//okCFrontPanel *dev;
//okTDeviceInfo  m_devInfo;

// Function declarations
bool isValidFilename(const std::string& filename);


// Function returns false if filename is empty
bool isValidFilename(const std::string& filename) {
    return !filename.empty();
}

// main follows unix standard where returning 0 is good
// accept a filename argument and switches start with "-" or "--"
int main(int argc, char* argv[]) {

  // Filename initialized as an empty string
  /* const char* filename = ""; */
  std::string filename = "";
  bool useFile = false;
  
  // Boolean switches initialized to false
  bool skipESP = false;
  bool skipMP = false;

  
  // Parse arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    //const char* arg = argv[i];

    // If the filename hasn't been set yet and the argument doesn't start with a dash, treat it as the filename
    if (isValidFilename(arg) && arg[0] != '-') {
      filename = arg;
      useFile = true;
      std::cout << "isValidFileName result: " << isValidFilename(filename) << std::endl;
    } else if (arg == "--skipESP" || arg == "-skipESP") {
      skipESP = true;
    } else if (arg == "--skipMP" || arg == "-skipMP") {
      skipMP = true;
    } else {
      std::cerr << "Unknown option: " << arg << std::endl;
    }
  }

  // Output the parsed information
  std::cout << "filename: " << (isValidFilename(filename) ? filename : "None") << "\n";
  std::cout << "skipESP: " << (skipESP ? "ON" : "OFF") << std::endl;
  std::cout << "skipMP: " << (skipMP ? "ON" : "OFF") << "\n";

  std::cout << "variable useFile: " << (useFile) << std::endl;
  
  auto start = std::chrono::system_clock::now();
  // Some computation here

  auto end = std::chrono::system_clock::now();
 
  std::chrono::duration<double> elapsed_seconds = end-start;
  // to convert the clock time to something like 
  std::time_t end_time = std::chrono::system_clock::to_time_t(end);
  std::cout <<  "finished at" << std::ctime(&end_time) << std::endl;
    
  std::cout << "elapsed time: " << elapsed_seconds.count() << " sec"
	    << std::endl;

  return 0;
}
