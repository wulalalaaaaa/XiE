#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Engine {

    void Log::Init() {
        m_enginelogger = spdlog::stdout_color_mt("enginelogger");
        m_enginelogger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        m_clientlogger = spdlog::stdout_color_mt("clientlogger");
        m_clientlogger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        spdlog::set_default_logger(m_clientlogger);
        spdlog::set_level(spdlog::level::trace);
        spdlog::info("[XLog] Initialized with spdlog");
    }

    void Log::log(spdlog::level::level_enum level, const std::string& message, const char* file, int line) {
        if (file != nullptr) {
            spdlog::log(level, "{} ({}:{})", message, file, line);
        } else {
            spdlog::log(level, "{}", message);
        }
    }

} // namespace Engine
