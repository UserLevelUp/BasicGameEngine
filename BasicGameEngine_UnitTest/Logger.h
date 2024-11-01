#ifndef LOGGER_H
#define LOGGER_H

#ifdef UNIT_TEST  // Only include this code when UNIT_TEST is defined

#include <fstream>
#include <string>
#include <iostream>
#include <mutex>  // Include for thread safety

class Logger {
public:
    // Singleton pattern to manage a single instance of the logger
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    // Function to log messages
    void Log(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);  // Ensure thread safety
        if (logFile.is_open()) {
            try {
                logFile << message << std::endl;
                logFile.flush();  // Ensure the log is written immediately
                std::cerr << "Logged message: " << message << std::endl;  // Debugging output to console
            }
            catch (const std::exception& ex) {
                std::cerr << "Exception during logging: " << ex.what() << std::endl;
            }
        }
        else {
            std::cerr << "Log file is not open." << std::endl;  // Error if log file is not open
        }
    }

private:
    std::ofstream logFile;
    std::mutex mutex_;  // Mutex to ensure thread safety

    // Private constructor to initialize the log file
    Logger() {
        try {
            logFile.open("command_history_log.txt", std::ios::out | std::ios::app);
            if (!logFile.is_open()) {
                std::cerr << "Failed to open log file." << std::endl;
            }
            else {
                std::cerr << "Log file opened successfully." << std::endl;  // Confirm the file opened
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception opening log file: " << ex.what() << std::endl;
        }
    }

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Destructor to close the log file
    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }
};

#else  // If UNIT_TEST is not defined, provide an empty logger

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Log(const std::string&) {}  // No-op for non-test builds

private:
    Logger() {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    ~Logger() {}
};

#endif  // UNIT_TEST

#endif // LOGGER_H
