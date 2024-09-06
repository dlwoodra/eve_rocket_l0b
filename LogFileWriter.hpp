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
#include "TimeInfo.hpp"

class LogFileWriter {
public:
    static LogFileWriter& getInstance() {
        static LogFileWriter instance;
        return instance;
    }

    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);

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
