//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <iostream>
#include <format>
#include <string_view>
#include <filesystem>

namespace ref_storage::utils {

    enum class LogLevel {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    struct LogEvent {
        std::string consoleMsg;
        std::string fileMsg;
    };

    class AsyncLogger {
    public:
        static AsyncLogger& getInstance();

        void init(const std::string& filename, LogLevel minLevel = LogLevel::Info);
        void stop();
        void emergencyFlush();
        void consumeLogs();

        template <typename... Args>
        void log(LogLevel level, const char* file, int line, std::string_view fmt, Args&&... args) {
            if (level < m_minLevel || !m_running) return;
            std::string message = std::vformat(fmt, std::make_format_args(args...));
            enqueueLog(level, file, line, message);
        }

        template <typename... Args>
        void syncLog(LogLevel level, const char* file, int line, std::string_view fmt, Args&&... args) {
            if (level < m_minLevel) return;
            std::string message = std::vformat(fmt, std::make_format_args(args...));
            writeSync(level, file, line, message);
        }

        AsyncLogger(const AsyncLogger&) = delete;
        AsyncLogger& operator=(const AsyncLogger&) = delete;

    private:
        AsyncLogger();
        ~AsyncLogger();

        void enqueueLog(LogLevel level, const char* file, int line, const std::string& message);
        void writeSync(LogLevel level, const char* file, int line, const std::string& message);
        void rotateLog();

        std::string getTimestamp();
        std::string levelToString(LogLevel level);
        const char* getLevelColor(LogLevel level);

        LogLevel m_minLevel;
        std::ofstream m_fileStream;

        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::queue<LogEvent> m_queue;
        std::atomic<bool> m_running;

        std::string m_baseFilename;
        size_t m_maxFileSize;
        size_t m_currentFileSize;
    };

} // namespace ref_storage::utils

#define LOG_DEBUG(fmt, ...) ref_storage::utils::AsyncLogger::getInstance().log(ref_storage::utils::LogLevel::Debug, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(fmt, ...)  ref_storage::utils::AsyncLogger::getInstance().log(ref_storage::utils::LogLevel::Info,  __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARN(fmt, ...)  ref_storage::utils::AsyncLogger::getInstance().log(ref_storage::utils::LogLevel::Warn,  __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(fmt, ...) ref_storage::utils::AsyncLogger::getInstance().log(ref_storage::utils::LogLevel::Error, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_FATAL(fmt, ...) do { \
    ref_storage::utils::AsyncLogger::getInstance().log(ref_storage::utils::LogLevel::Fatal, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__); \
    ref_storage::utils::AsyncLogger::getInstance().emergencyFlush(); \
    std::abort(); \
} while(0)

#define LOG_SYNC_DEBUG(fmt, ...) ref_storage::utils::AsyncLogger::getInstance().syncLog(ref_storage::utils::LogLevel::Debug, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_SYNC_INFO(fmt, ...)  ref_storage::utils::AsyncLogger::getInstance().syncLog(ref_storage::utils::LogLevel::Info,  __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_SYNC_WARN(fmt, ...)  ref_storage::utils::AsyncLogger::getInstance().syncLog(ref_storage::utils::LogLevel::Warn,  __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_SYNC_ERROR(fmt, ...) ref_storage::utils::AsyncLogger::getInstance().syncLog(ref_storage::utils::LogLevel::Error, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_SYNC_FATAL(fmt, ...) do { \
    ref_storage::utils::AsyncLogger::getInstance().syncLog(ref_storage::utils::LogLevel::Fatal, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__); \
    std::abort(); \
} while(0)

#endif // ASYNC_LOGGER_HPP