//
// This code read data from a serial port and send it to a client over a TCP socket.
// Multiple clients can connect to the server and receive the data simultaneously.
// Usage: ./surfSerialServer
// Usage2: ./surfSerialServer --test
// The --test flag can be used to read data from a test_input.txt file instead of the serial port.
// The serial port name and server port number are defined as constants at the top of the file.
// The server will log all received data to a surf_log.txt file.
// The server can be stopped by pressing Ctrl+C.
//
// To test, start the server using ./surfSerialServer --test and connect to us using netcat:    
// nc localhost 55011
// This should display the data being sent by the server.

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define SERVER_PORT_NUMBER 55011
#define SERIAL_PORT_NAME "/dev/ttyUSB0"

std::mutex fileMutex;
std::mutex serialMutex;
std::atomic<bool> running(true);
bool useFileInput = false;
std::ifstream testInputFile;
std::vector<int> clients;  // Store connected client sockets
std::mutex clientsMutex;

std::atomic<bool> shutdownRequested(false);
int serverSocket = -1; // Store server socket globally for signal handler

void signalHandler(int signum) {
    running = false;
    std::cout << "Shutting down server..." << std::endl;
    shutdownRequested = true; // Set flag to close all client connections and stop main loop

    // Close all client connections
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (int client : clients) {
        close(client);
    }

    if (serverSocket != -1) {
        close(serverSocket); // Close server socket
    }
    std::exit(0); // Exit the program
}

// Function to read one line from either the serial port or test file
std::string readLineFromSource(int serialFd) {
    std::string line;
    char c;

    auto isValidChar = [](char ch) -> bool {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') || (ch == '+' || ch == '-' || ch == '.' || ch == '/') ||
               (ch == ':' || ch == '"' || ch == ' ' || ch == '_');
    };

    if (useFileInput) {
        std::lock_guard<std::mutex> lock(serialMutex);
        if (std::getline(testInputFile, line)) {
            // Validate and filter characters
            std::string filteredLine;
            for (char ch : line) {
                if (isValidChar(ch)) {
                    filteredLine += ch;
                }
            }
            return filteredLine;
        } else {
            // do not stop if end of file is reached, start over
            testInputFile.clear();
            testInputFile.seekg(0, std::ios::beg); // Restart file for continuous testing
        }
    } else {
        std::lock_guard<std::mutex> lock(serialMutex);
        while (read(serialFd, &c, 1) > 0) {
            if (c == '\n') {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                break;
            } else if (isValidChar(c)) {
                line += c;
            }
        }
    }
    return line;
}

// Function to handle a client connection
void handleClient(int clientSocket) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back(clientSocket);
    }

    char buffer[1024];  // Temporary buffer for receiving data
    while (running) {
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            // Client disconnected or error occurred
            std::cerr << "Client disconnected or error: " << strerror(errno) << std::endl;
            break;
        }

        buffer[bytesReceived] = '\0';  // Ensure null termination
        std::cout << "Received from client: " << buffer << std::endl;
    }

    close(clientSocket);

    // Remove client from the list
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::find(clients.begin(), clients.end(), clientSocket);
        if (it != clients.end()) {
            clients.erase(it);
        }
    }

    std::cout << "Client handler thread exiting\n";
}

// Function to read data from the serial port and broadcast it to all connected clients
void broadcastLoop(int serialFd, std::ofstream& surfLogFile) {
    while (running) {
        std::string line = readLineFromSource(serialFd);
        if (!line.empty()) {
            {
                std::lock_guard<std::mutex> lock(fileMutex);
                surfLogFile << line << std::endl;
                surfLogFile.flush();
            }

            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto it = clients.begin(); it != clients.end();) {
                if (send(*it, line.c_str(), line.size(), 0) < 0) {
                    std::cerr << "Client disconnected: " << strerror(errno) << std::endl;
                    close(*it);
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (useFileInput) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);

    if (argc > 1 && std::string(argv[1]) == "--test") {
        useFileInput = true;
    }

    int serialFd = -1;
    testInputFile.open("test_input.txt");

    if (testInputFile.is_open()) {
        useFileInput = true;
        std::cout << "Using test_input.txt as data source." << std::endl;
    } else {
        serialFd = open(SERIAL_PORT_NAME, O_RDWR | O_NOCTTY | O_SYNC);
        if (serialFd < 0) {
            std::cerr << "Error opening serial port: " << strerror(errno) << std::endl;
            return 1;
        }
        useFileInput = false;
    }

    std::ofstream surfLogFile("surf_log.txt", std::ios::app);
    if (!surfLogFile.is_open()) {
        std::cerr << "Error opening log file\n";
        return 1;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return 1;
    }

    // Enable SO_REUSEADDR to avoid "address already in use" error 
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
        return 1;
    }

    if (!useFileInput) {
        struct termios tty {};
        if (tcgetattr(serialFd, &tty) != 0) {
            std::cerr << "Error getting serial port attributes: " << strerror(errno) << std::endl;
            close(serialFd);
            exit(1);
        }

        tty.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
        tty.c_iflag = IGNPAR;
        tty.c_oflag = 0;
        tty.c_lflag = 0;
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 20;

        if (tcflush(serialFd, TCIFLUSH) != 0 || tcsetattr(serialFd, TCSANOW, &tty) != 0) {
            std::cerr << "Error configuring serial port: " << strerror(errno) << std::endl;
            close(serialFd);
            exit(1);
        }
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT_NUMBER);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << SERVER_PORT_NUMBER << std::endl;

    std::thread broadcaster(broadcastLoop, serialFd, std::ref(surfLogFile));

    while (!shutdownRequested) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (errno == EINTR) {
                continue;  // Try again if interrupted
            }
            std::cerr << "Error accepting client connection: " << strerror(errno) << std::endl;
            break;  // Break the loop on fatal errors
        }

        std::cout << "Client connected\n";
        std::thread(handleClient, clientSocket).detach();
    }

    broadcaster.join();

    // Cleanup
    surfLogFile.close();
    if (!useFileInput) {
        close(serialFd);
    }

    std::cout << "Server shutting down\n";
    close(serverSocket);

    return 0;
}



// #include <iostream>
// #include <fstream>
// #include <thread>
// #include <vector>
// #include <mutex>
// #include <atomic>
// #include <cstring>
// #include <csignal>
// #include <arpa/inet.h>
// #include <termios.h>
// #include <unistd.h>
// #include <fcntl.h>

// #define SERVER_PORT_NUMBER 55011
// #define SERIAL_PORT_NAME "/dev/ttyUSB0"
// //#define PORT 55011

// std::mutex fileMutex;
// std::mutex serialMutex;
// std::atomic<bool> running(true);

// void signalHandler(int signum) {
//     running = false;
// }

// std::string readLineFromSerial(int serialFd) {
//     std::string line;
//     char c;

//     auto isValidChar = [](char ch) -> bool {
//         return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
//                (ch >= '0' && ch <= '9') || (ch == '+' || ch == '-' || ch == '.' || ch == '/') ||
//                (ch == ':' || ch == '"' || ch == ' ' || ch == '_');
//     };

//     {
//         std::lock_guard<std::mutex> lock(serialMutex); // Protect serial read
//         while (read(serialFd, &c, 1) > 0) {
//             if (c == '\n') {
//                 if (!line.empty() && line.back() == '\r') {
//                     line.pop_back();
//                 }
//                 break;
//             } else if (isValidChar(c)) {
//                 line += c;
//             }
//         }
//     }

//     return line;
// }

// void handleClient(int clientSocket, int serialFd, std::ofstream& surfLogFile) {
//     while (running) {
//         std::string line = readLineFromSerial(serialFd);
//         if (!line.empty()) {
//             {
//                 std::lock_guard<std::mutex> lock(fileMutex);
//                 surfLogFile << line << std::endl;
//                 surfLogFile.flush();
//             }

//             if (send(clientSocket, line.c_str(), line.size(), 0) < 0) {
//                 std::cerr << "Error sending to client: " << strerror(errno) << std::endl;
//                 break;
//             }
//         } else {
//             break;
//         }
//     }

//     close(clientSocket);
// }

// int main() {
//     signal(SIGINT, signalHandler);

//     int serialFd = open(SERIAL_PORT_NAME, O_RDWR | O_NOCTTY | O_SYNC);
//     if (serialFd < 0) {
//         std::cerr << "Error opening serial port: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     std::ofstream surfLogFile("surf_log.txt", std::ios::app);
//     if (!surfLogFile.is_open()) {
//         std::cerr << "Error opening log file\n";
//         return 1;
//     }

//     int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (serverSocket < 0) {
//         std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     struct termios tty {};
//     if (tcgetattr(serialFd, &tty) != 0) {
//         std::cerr << "Error getting serial port attributes: " << strerror(errno) << std::endl;
//         close(serialFd);
//         exit(1);
//     }

//     tty.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
//     tty.c_iflag = IGNPAR;
//     tty.c_oflag = 0;
//     tty.c_lflag = 0;
//     tty.c_cc[VMIN] = 1;
//     tty.c_cc[VTIME] = 20;

//     if (tcflush(serialFd, TCIFLUSH) != 0 || tcsetattr(serialFd, TCSANOW, &tty) != 0) {
//         std::cerr << "Error configuring serial port: " << strerror(errno) << std::endl;
//         close(serialFd);
//         exit(1);
//     }


//     sockaddr_in serverAddr = {};
//     serverAddr.sin_family = AF_INET;
//     serverAddr.sin_addr.s_addr = INADDR_ANY;
//     serverAddr.sin_port = htons(SERVER_PORT_NUMBER);

//     if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
//         std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     if (listen(serverSocket, 5) < 0) {
//         std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     std::cout << "Server listening on port " << SERIAL_PORT_NAME << std::endl;

//     std::vector<std::thread> clientThreads;

//     while (running) {
//         sockaddr_in clientAddr;
//         socklen_t clientLen = sizeof(clientAddr);
//         int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
//         if (clientSocket < 0) {
//             if (running) {
//                 std::cerr << "Error accepting client connection: " << strerror(errno) << std::endl;
//             }
//             break;
//         }

//         std::cout << "Client connected\n";

//         clientThreads.emplace_back(handleClient, clientSocket, serialFd, std::ref(surfLogFile));
//     }

//     // Cleanup
//     close(serverSocket);
//     close(serialFd);
//     surfLogFile.close();

//     for (auto& t : clientThreads) {
//         if (t.joinable()) {
//             t.join();
//         }
//     }

//     std::cout << "Server shutting down\n";
//     return 0;
// }


// #include <iostream>
// #include <fstream>
// #include <string>
// #include <chrono>
// #include <iomanip>
// #include <sstream>
// //#include <atomic>
// #include <ctime>
// #include <unistd.h>
// #include <fcntl.h>
// #include <termios.h>
// #include <thread>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <cstring>
// #include <mutex>
// #include <vector>

// #define SERVER_PORT 55011
// #define SERIAL_PORT "/dev/ttyUSB0"

// std::mutex fileMutex;
// std::mutex serialMutex;

// // Function to read a complete line from the serial port and log it to a file
// std::string readLineFromSerial(int serialFd, std::ofstream& surfLogFile) {
//     std::string line;
//     char c;

//     auto isValidChar = [](char ch) -> bool {
//         return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
//                (ch >= '0' && ch <= '9') || (ch == '+' || ch == '-' || ch == '.' || ch == '/') ||
//                (ch == ':' || ch == '"' || ch == ' ' || ch == '_');
//     };

//     {
//         std::lock_guard<std::mutex> lock(serialMutex); // Protect serial read
//         while (read(serialFd, &c, 1) > 0) {
//             if (c == '\n') {
//                 if (!line.empty() && line.back() == '\r') {
//                     line.pop_back();
//                 }
//                 break;
//             } else if (isValidChar(c)) {
//                 line += c;
//             }
//         }
//     }

//     return line;
// }

// // Function to handle a client connection
// void handleClient(int clientSocket, int serialFd, std::ofstream& surfLogFile) {
//     while (true) {
//         std::string line = readLineFromSerial(serialFd, surfLogFile);
//         if (!line.empty()) {
//             // Lock file access
//             {
//                 std::lock_guard<std::mutex> lock(fileMutex);
//                 surfLogFile << line << std::endl;
//                 surfLogFile.flush();
//             }

//             // Send the line to the client
//             if (send(clientSocket, line.c_str(), line.size(), 0) < 0) {
//                 std::cerr << "Error sending to client: " << strerror(errno) << std::endl;
//                 break;
//             }
//         } else {
//             std::cerr << "Error or EOF reading from serial port\n";
//             break;
//         }
//     }

//     close(clientSocket);
// }

// // Function to setup the serial port
// void setupSerialPort(int &serialFd) {
//     serialFd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
//     if (serialFd < 0) {
//         std::cerr << "Error opening serial port: " << strerror(errno) << std::endl;
//         exit(1);
//     }

//     struct termios tty {};
//     if (tcgetattr(serialFd, &tty) != 0) {
//         std::cerr << "Error getting serial port attributes: " << strerror(errno) << std::endl;
//         close(serialFd);
//         exit(1);
//     }

//     tty.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
//     tty.c_iflag = IGNPAR;
//     tty.c_oflag = 0;
//     tty.c_lflag = 0;
//     tty.c_cc[VMIN] = 1;
//     tty.c_cc[VTIME] = 20;

//     if (tcflush(serialFd, TCIFLUSH) != 0 || tcsetattr(serialFd, TCSANOW, &tty) != 0) {
//         std::cerr << "Error configuring serial port: " << strerror(errno) << std::endl;
//         close(serialFd);
//         exit(1);
//     }
// }

// // Function to create a timestamped filename
// std::string generateFilename() {
//     auto now = std::chrono::system_clock::now();
//     std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
//     std::tm* localTime = std::localtime(&timeNow);

//     std::ostringstream filename;
//     filename << "surf_RT_serial_data_"
//              << std::put_time(localTime, "%Y%m%d_%H%M%S") // Format: YYYYMMDD_HHMMSS
//              << ".txt";
//     return filename.str();
// }

// int main() {
//     int serverSocket, clientSocket;
//     sockaddr_in serverAddr {};
//     sockaddr_in clientAddr {};
//     socklen_t clientLen = sizeof(clientAddr);
//     int serialFd;
//     std::string surfLogFilename = generateFilename();
//     std::ofstream surfLogFile(surfLogFilename, std::ios::app);

//     if (!surfLogFile.is_open()) {
//         std::cerr << "Error opening log file" << std::endl;
//         return 1;
//     }

//     // Setup serial port
//     setupSerialPort(serialFd);

//     // Create server socket
//     serverSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (serverSocket < 0) {
//         std::cerr << "Error creating server socket: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     serverAddr.sin_family = AF_INET;
//     serverAddr.sin_addr.s_addr = INADDR_ANY;
//     serverAddr.sin_port = htons(SERVER_PORT);

//     if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
//         std::cerr << "Error binding server socket: " << strerror(errno) << std::endl;
//         close(serverSocket);
//         return 1;
//     }

//     if (listen(serverSocket, 2) < 0) {
//         std::cerr << "Error listening on server socket: " << strerror(errno) << std::endl;
//         close(serverSocket);
//         return 1;
//     }

//     std::cout << "Server listening on port " << SERVER_PORT << "...\n";

//     std::vector<std::thread> clientThreads;

//     while (true) {
//         clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
//         if (clientSocket < 0) {
//             std::cerr << "Error accepting client connection: " << strerror(errno) << std::endl;
//             break;
//         }

//     std::cout << "Client connected\n";

//     clientThreads.emplace_back(handleClient, clientSocket, serialFd, std::ref(surfLogFile));
//     }

//     // Proper cleanup (after exiting the loop)
//     for (auto& t : clientThreads) {
//         if (t.joinable()) {
//             t.join();
//         }
//     }

//     // while (true) {
//     //     clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
//     //     if (clientSocket < 0) {
//     //         std::cerr << "Error accepting client connection: " << strerror(errno) << std::endl;
//     //         break;
//     //     }

//     //     std::cout << "Client connected\n";

//     //     // Handle client in a separate thread
//     //     std::thread clientThread(handleClient, clientSocket, serialFd, std::ref(surfLogFile));
//     //     clientThread.detach();
//     // }

//     close(serverSocket);
//     close(serialFd);
//     surfLogFile.close();

//     return 0;
// }
