#ifndef LOGFILEWRITER_HPP
#define LOGFILEWRITER_HPP

// uses Singleton pattern

//#include <spdlog/spdlog.h>
//#include <spdlog/sinks/basic_file_sink.h>
#include "spdlog_pch.hpp"
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include "TimeInfo.hpp"

class LogFileWriter {
public:
    static LogFileWriter& getInstance() {
        static LogFileWriter instance;
        return instance;
    }

    // Templates have to be implemented in the class or other code will fail to link

    // Template-based logInfo with formatting support
    template<typename... Args>
    void logInfo(const std::string& message, Args&&... args) {
        checkAndRotateFile();
        logger->info(message, std::forward<Args>(args)...);
    }

    // Template-based logWarning with formatting support
    template<typename... Args>
    void logWarning(const std::string& message, Args&&... args)  {
        checkAndRotateFile();
        logger->warn(message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logError(const std::string& message, Args&&... args)  {
        checkAndRotateFile();
        logger->error(message, std::forward<Args>(args)...);
    }

    // Add a getter for the current log file path
    const std::string& getLogFilePath() const {
        return logFile;
    }
    
    void close();

private:
    LogFileWriter();
    ~LogFileWriter();

    std::shared_ptr<spdlog::logger> logger;
    std::string logFile;
    int logFileMinute;
    bool checkAndRotateFile();
    std::string generateLogFilename() const;

    // Delete copy constructor and assignment operator
    LogFileWriter(const LogFileWriter&) = delete;
    LogFileWriter& operator=(const LogFileWriter&) = delete;
};

#endif // LOGFILEWRITER_HPP
