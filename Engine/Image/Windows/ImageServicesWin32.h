#pragma once

#include "Image/ImageServices.h"

#include <memory>

namespace Engine {

// Creates the optional Windows image service set.  The public return type is
// portable; WIC, COM and clipboard handles remain private to the backend.
std::shared_ptr<ImageServices> CreateWin32ImageServices();

} // namespace Engine
