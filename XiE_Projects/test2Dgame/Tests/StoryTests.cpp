#include "Story/StoryFileCatalog.h"
#include "Story/StorySelfTest.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::filesystem::path storyRoot =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("Assets/Story");

    Engine::Story::StoryFileCatalog catalog;
    if (!catalog.Load(storyRoot)) {
        std::cerr << "Story catalog load FAILED: " << catalog.LastError() << '\n';
        return 1;
    }

    std::cout << "Story catalog load PASSED: stories=" << catalog.StoryKeys().size()
              << " contentVersion=" << catalog.ContentVersion() << '\n';

    const Test2D::StorySelfTestResult result = Test2D::RunStorySelfTest(catalog);
    std::cout << "Story runtime/loopback self-test "
              << (result.passed ? "PASSED" : "FAILED")
              << ": checks=" << result.checksPassed
              << " message=" << result.message << '\n';
    return result.passed ? 0 : 2;
}
