//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#include "utils/include/AsyncLogger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <thread>

namespace ref_storage::utils {

    constexpr const char* COLOR_RESET = "\033[0m";
    constexpr const char* COLOR_CYAN  = "\033[36m";
    constexpr const char* COLOR_DEBUG = "\033[34m";
    constexpr const char* COLOR_INFO  = "\033[32m";
    constexpr const char* COLOR_WARN  = "\033[33m";
    constexpr const char* COLOR_ERROR = "\033[31m";
    constexpr const char* COLOR_FATAL = "\033[1;31m";

    extern "C" void crashSignalHandler(int signum) {
        AsyncLogger::getInstance().emergencyFlush();
        std::signal(signum, SIG_DFL);
        std::raise(signum);
    }

    AsyncLogger& AsyncLogger::getInstance() {
        static AsyncLogger instance;
        return instance;
    }

    AsyncLogger::AsyncLogger()
        : m_minLevel(LogLevel::Info),
          m_running(false),
          m_maxFileSize(10 * 1024 * 1024),
          m_currentFileSize(0) {
    }

    AsyncLogger::~AsyncLogger() {
        stop();
        if (m_fileStream.is_open()) m_fileStream.close();
    }

    void AsyncLogger::init(const std::string& filename, LogLevel minLevel) {
        m_minLevel = minLevel;
        m_baseFilename = filename;

        m_fileStream.open(m_baseFilename, std::ios::app);
        if (!m_fileStream.is_open()) {
            std::cout << "\n[LOGGER WARNING] Failed to open log file. Disk logging disabled.\n" << std::flush;
            m_currentFileSize = 0;
        } else {
            try {
                m_currentFileSize = static_cast<size_t>(std::filesystem::file_size(m_baseFilename));
            } catch (const std::filesystem::filesystem_error&) {
                m_currentFileSize = 0;
            }
        }

        std::signal(SIGSEGV, crashSignalHandler);
        std::signal(SIGABRT, crashSignalHandler);
        std::signal(SIGILL,  crashSignalHandler);
        std::signal(SIGFPE,  crashSignalHandler);

        m_running = true;
    }

    void AsyncLogger::stop() {
        if (m_running) {
            m_running = false;
            m_cv.notify_all();
        }
    }

    std::string AsyncLogger::getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
        return ss.str();
    }

    std::string AsyncLogger::levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    const char* AsyncLogger::getLevelColor(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return COLOR_DEBUG;
            case LogLevel::Info:  return COLOR_INFO;
            case LogLevel::Warn:  return COLOR_WARN;
            case LogLevel::Error: return COLOR_ERROR;
            case LogLevel::Fatal: return COLOR_FATAL;
            default: return COLOR_RESET;
        }
    }

    void AsyncLogger::rotateLog() {
        if (m_fileStream.is_open()) m_fileStream.close();

        std::string timestamp = getTimestamp();
        for (char &c : timestamp) if (c == ' ' || c == ':' || c == '.') c = '_';

        std::string backupName = m_baseFilename + "." + timestamp + ".bak";
        try { std::filesystem::rename(m_baseFilename, backupName); }
        catch (...) { /* 忽略重命名失败 */ }

        m_fileStream.open(m_baseFilename, std::ios::out | std::ios::trunc);
        m_currentFileSize = 0;
    }

    void AsyncLogger::enqueueLog(LogLevel level, const char* file, int line, const std::string& message) {
        std::string timestamp = getTimestamp();
        std::stringstream ss;
        ss << std::this_thread::get_id();
        std::string threadId = ss.str();

        std::string fileMsg = std::format("[T:{}] [{}] [{}] [{}:{}] {}\n",
                                          threadId, timestamp, levelToString(level), file, line, message);

        std::string consoleMsg = std::format("[{}T:{}{}] [{}{}{}] [{}{}{}] [{}:{}] {}\n",
            COLOR_CYAN, threadId, COLOR_RESET, COLOR_CYAN, timestamp, COLOR_RESET,
            getLevelColor(level), levelToString(level), COLOR_RESET, file, line, message);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push({std::move(consoleMsg), std::move(fileMsg)});
        }
        m_cv.notify_one();
    }

    void AsyncLogger::writeSync(LogLevel level, const char* file, int line, const std::string& message) {
        std::string timestamp = getTimestamp();
        std::stringstream ss;
        ss << std::this_thread::get_id();

        std::string fileMsg = std::format("[T:{}] [{}] [{}] [{}:{}] {}\n",
                                          ss.str(), timestamp, levelToString(level), file, line, message);
        std::string consoleMsg = std::format("[{}T:{}{}] [{}{}{}] [{}{}{}] [{}:{}] {}\n",
            COLOR_CYAN, ss.str(), COLOR_RESET, COLOR_CYAN, timestamp, COLOR_RESET,
            getLevelColor(level), levelToString(level), COLOR_RESET, file, line, message);

        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << consoleMsg << std::flush;

        if (m_fileStream.is_open()) {
            m_fileStream << fileMsg;
            m_fileStream.flush();
            m_currentFileSize += fileMsg.size();
            if (m_currentFileSize >= m_maxFileSize) rotateLog();
        }
    }

    void AsyncLogger::consumeLogs() {
        std::queue<LogEvent> localQueue;
        while (m_running || !m_queue.empty()) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]{ return !m_queue.empty() || !m_running; });
                if (m_queue.empty() && !m_running) break;
                localQueue.swap(m_queue);
            }

            bool wroteAnything = false;
            while (!localQueue.empty()) {
                const auto& event = localQueue.front();
                std::cout << event.consoleMsg << std::flush;
                if (m_fileStream.is_open()) {
                    m_fileStream << event.fileMsg;
                    m_currentFileSize += event.fileMsg.size();
                    if (m_currentFileSize >= m_maxFileSize) rotateLog();
                }
                localQueue.pop();
                wroteAnything = true;
            }
            if (wroteAnything && m_fileStream.is_open()) m_fileStream.flush();
        }
    }

    void AsyncLogger::emergencyFlush() {
        if (m_mutex.try_lock()) {
            while (!m_queue.empty()) {
                std::cout << m_queue.front().consoleMsg << std::flush;
                if (m_fileStream.is_open()) m_fileStream << m_queue.front().fileMsg;
                m_queue.pop();
            }
            m_mutex.unlock();
        }
        if (m_fileStream.is_open()) {
            m_fileStream << "\n--- CRASH INTERCEPTED ---\n";
            m_fileStream.flush();
        }
    }
} // namespace ref_storage::utils