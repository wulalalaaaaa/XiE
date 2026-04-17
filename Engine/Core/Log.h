#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace Engine {


    static std::shared_ptr<spdlog::logger> m_enginelogger;
    static std::shared_ptr<spdlog::logger> m_clientlogger;

    class Log {
    public:
        enum class Level {
            Info,
            Warn,
            Error
        };

        static void Init();

        static void log(spdlog::level::level_enum level, const std::string& message, const char* file, int line);
    
    };

} // namespace Engine

#define XLOG(level, message) Engine::Log::log((level), (message), __FILE__, __LINE__)
#define XLOG_INFO(message)    XLOG(spdlog::level::info, (message))
#define XLOG_WARN(message)    XLOG(spdlog::level::warn, (message))
#define XLOG_ERROR(message)   XLOG(spdlog::level::err, (message))
