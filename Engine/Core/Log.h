#pragma once

#include <string>

namespace Engine {

class Log {
public:
    static void Init();
    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);
};

} // namespace Engine
