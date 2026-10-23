#pragma once

#include <string>

namespace Engine::Story {
class IStoryCatalog;
}

namespace Test2D {

struct StorySelfTestResult {
    bool passed = false;
    int checksPassed = 0;
    std::string message;
};

StorySelfTestResult RunStorySelfTest(const Engine::Story::IStoryCatalog& catalog);

} // namespace Test2D
