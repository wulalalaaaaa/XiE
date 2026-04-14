#include "Log.h"

#include <iostream>

namespace Engine {

void Log::Init() {
    std::cout << "[Log] Initialized" << std::endl;
}

void Log::Info(const std::string& message) {
    std::cout << "[Info] " << message << std::endl;
}

void Log::Warn(const std::string& message) {
    std::cout << "[Warn] " << message << std::endl;
}

void Log::Error(const std::string& message) {
    std::cerr << "[Error] " << message << std::endl;
}

} // namespace Engine
