#include "LogFileWriter.hpp"

LogFileWriter::LogFileWriter()
    : logFile(generateLogFilename()), logFileMinute(-1) {
    logger = spdlog::basic_logger_mt("log_file_logger", logFile);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");

    // Initialize logFileMinute
    TimeInfo currentTime;
    logFileMinute = currentTime.getMinute();

    std::cout << "Log file opened: " << logFile << std::endl;
}

LogFileWriter::~LogFileWriter() {
    // No explicit cleanup needed for spdlog
}

bool LogFileWriter::checkAndRotateFile() {
    TimeInfo currentTime;
    int currentMinute = currentTime.getMinute();

    if (logFileMinute == -1 || logFileMinute != currentMinute) {
        // Minute has changed, rotate log file
        spdlog::drop("log_file_logger"); // Drop the old logger
        logFile = generateLogFilename(); // Generate new filename
        logger = spdlog::basic_logger_mt("log_file_logger", logFile);
        logger->set_level(spdlog::level::info);
        logger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");

        logFileMinute = currentMinute;
        std::cout << "Log file rotated: " << logFile << std::endl;
    }

    return true;
}

std::string LogFileWriter::generateLogFilename() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm buf;
    localtime_r(&in_time_t, &buf);

    std::ostringstream oss;

    // create directory
    oss << std::put_time(&buf, "./logs/%Y/%j/");
    std::string dirPath = oss.str();
    // Create the directories if they don't exist, use system call
    std::string mkdirCommand = "mkdir -p " + dirPath;
    if (system(mkdirCommand.c_str()) != 0) {
        std::cerr << "ERROR: Could not create directories for log file." << std::endl;
        return "";
    }

    oss << std::put_time(&buf, "log_%Y_%j_%m_%d_%H_%M_%S") << ".log";

    return oss.str();
}
