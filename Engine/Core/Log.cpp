#include "Log.h"

#include <iostream>
#include <string>

namespace Engine {

namespace {

const char* ToTag(Log::Level level) {
    switch (level) {
    case Log::Level::Green:    return "[ √ ]";
    case Log::Level::OkBut:    return "[okBut]";
    case Log::Level::NotAllow: return "[notAllow]";
    case Log::Level::Warn:     return "[warning]";
    case Log::Level::Error:    return "[error]";
    case Log::Level::Info:     return "[info]";
    default:                   return "[ ??? ]";
    }
}

} // namespace

void Log::Init() {
    XLOG_GREEN("[XLog] Initialized.");
}

void Log::log(Log::Level level, const std::string& message, const char* file, int line) {
    std::ostream& out = (level == Level::Error) ? std::cerr : std::cout;
    out << ToTag(level) << ' ' << message;

    if (file != nullptr) {
        out << " (" << file;
        if (line > 0) {
            out << ':' << line;
        }
        out << ')';
    }

    out << "\n";
}

} // namespace Engine
