#pragma once

#include <string>

namespace Engine {

class Log {
public:
    enum class Level {
        Green,
        OkBut,
        NotAllow,
        Warn,
        Error,
        Info
    };

    static void Init();

    static void log(Level level, const std::string& message, const char* file = nullptr, int line = 0);
};

} // namespace Engine

#define XLOG(level, message) Engine::Log::log((level), (message), __FILE__, __LINE__)
#define XLOG_INFO(message)    XLOG(Engine::Log::Level::Info, (message))
#define XLOG_WARN(message)    XLOG(Engine::Log::Level::Warn, (message))
#define XLOG_ERROR(message)   XLOG(Engine::Log::Level::Error, (message))
#define XLOG_OKBUT(message)   XLOG(Engine::Log::Level::OkBut, (message))
#define XLOG_NOTALLOW(message) XLOG(Engine::Log::Level::NotAllow, (message))
#define XLOG_GREEN(message)   XLOG(Engine::Log::Level::Green, (message))
