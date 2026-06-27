#pragma once

#include "World2D.h"

#include <filesystem>
#include <string>

namespace Test2D {

bool LoadScene2D(const std::filesystem::path& scenePath, World2D& world, std::string& outError);

} // namespace Test2D
