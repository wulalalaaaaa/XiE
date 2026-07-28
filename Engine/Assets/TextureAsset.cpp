#include "TextureAsset.h"

#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace Engine {

namespace {

std::string TrimCopy(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool StartsWithToken(const std::string& value, const std::string& token) {
    if (value.size() <= token.size()) {
        return false;
    }

    if (value.rfind(token, 0) != 0) {
        return false;
    }

    return std::isspace(static_cast<unsigned char>(value[token.size()])) != 0;
}

std::string ExtractValueAfterToken(const std::string& line, const std::string& token) {
    return TrimCopy(line.substr(token.size()));
}

bool IsSupportedImageExtension(const std::filesystem::path& path) {
    const std::string ext = ToLowerCopy(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

#ifdef _WIN32
class ScopedCoUninitialize {
public:
    explicit ScopedCoUninitialize(bool active) : m_Active(active) {}
    ~ScopedCoUninitialize() {
        if (m_Active) CoUninitialize();
    }

    ScopedCoUninitialize(const ScopedCoUninitialize&) = delete;
    ScopedCoUninitialize& operator=(const ScopedCoUninitialize&) = delete;

private:
    bool m_Active = false;
};

bool DecodeImageWithWIC(
    const std::filesystem::path& imagePath,
    std::vector<unsigned char>& outPixels,
    int& outWidth,
    int& outHeight,
    std::string& outError
) {
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = (initHr == S_OK || initHr == S_FALSE);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
        outError = "CoInitializeEx failed";
        return false;
    }
    // Declared before the COM pointers so their Release calls run before
    // CoUninitialize when this function leaves through any return path.
    ScopedCoUninitialize coUninitialize(shouldUninitialize);

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.GetAddressOf())
    );
    if (FAILED(hr)) {
        outError = "Failed to create WIC factory";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        imagePath.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf()
    );
    if (FAILED(hr)) {
        outError = "Failed to decode image file";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        outError = "Failed to read first image frame";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) {
        outError = "Failed to create WIC format converter";
        return false;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) {
        outError = "Failed to convert image format to RGBA8";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        outError = "Image has invalid dimensions";
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    const std::size_t totalBytes = rowBytes * static_cast<std::size_t>(height);

    outPixels.resize(totalBytes);
    hr = converter->CopyPixels(nullptr, static_cast<UINT>(rowBytes), static_cast<UINT>(totalBytes), outPixels.data());
    if (FAILED(hr)) {
        outPixels.clear();
        outError = "Failed to copy image pixels";
        return false;
    }

    outWidth = static_cast<int>(width);
    outHeight = static_cast<int>(height);

    return true;
}
#endif

} // namespace

bool TextureAsset::LoadFromFile(const std::filesystem::path& path) {
    m_DescriptorPath.clear();
    m_SourceImagePath.clear();
    m_Pixels.clear();
    m_Width = 0;
    m_Height = 0;
    m_ChannelCount = 4;

    if (!std::filesystem::exists(path)) {
        XLOG_ERROR((std::string("Texture file not found: ") + path.string()).c_str());
        return false;
    }

    const std::string ext = ToLowerCopy(path.extension().string());
    if (ext == ".xtexture") {
        std::filesystem::path imagePath;
        if (!ParseXTextureFile(path, imagePath)) {
            return false;
        }
        m_DescriptorPath = path;
        return LoadFromImageFile(imagePath);
    }

    if (!IsSupportedImageExtension(path)) {
        XLOG_ERROR((std::string("Unsupported texture extension: ") + path.string()).c_str());
        return false;
    }

    return LoadFromImageFile(path);
}

const unsigned char* TextureAsset::GetPixels() const {
    return m_Pixels.empty() ? nullptr : m_Pixels.data();
}

int TextureAsset::GetWidth() const {
    return m_Width;
}

int TextureAsset::GetHeight() const {
    return m_Height;
}

int TextureAsset::GetChannelCount() const {
    return m_ChannelCount;
}

const std::filesystem::path& TextureAsset::GetDescriptorPath() const {
    return m_DescriptorPath;
}

const std::filesystem::path& TextureAsset::GetSourceImagePath() const {
    return m_SourceImagePath;
}

bool TextureAsset::LoadFromImageFile(const std::filesystem::path& imagePath) {
    if (!std::filesystem::exists(imagePath)) {
        XLOG_ERROR((std::string("Texture image not found: ") + imagePath.string()).c_str());
        return false;
    }

    if (!IsSupportedImageExtension(imagePath)) {
        XLOG_ERROR((std::string("Texture image must be .png/.jpg/.jpeg: ") + imagePath.string()).c_str());
        return false;
    }

    std::vector<unsigned char> pixels;
    std::string error;
    int width = 0;
    int height = 0;

#ifdef _WIN32
    if (!DecodeImageWithWIC(imagePath, pixels, width, height, error)) {
        const std::string message = "Failed to load texture image: " + imagePath.string() + " (" + error + ")";
        XLOG_ERROR(message.c_str());
        return false;
    }
#else
    (void)pixels;
    (void)width;
    (void)height;
    error = "Image decoding is only implemented for Windows in this build";
    const std::string message = "Failed to load texture image: " + imagePath.string() + " (" + error + ")";
    XLOG_ERROR(message.c_str());
    return false;
#endif

    m_SourceImagePath = imagePath;
    m_Pixels = std::move(pixels);
    m_Width = width;
    m_Height = height;
    m_ChannelCount = 4;
    return true;
}

bool TextureAsset::ParseXTextureFile(const std::filesystem::path& descriptorPath, std::filesystem::path& outImagePath) {
    std::ifstream input(descriptorPath);
    if (!input.is_open()) {
        XLOG_ERROR((std::string("Failed to open xtexture file: ") + descriptorPath.string()).c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    std::istringstream stream(content);
    std::string line;
    bool sawHeader = false;
    std::string imageValue;

    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (!sawHeader) {
            if (trimmed != "xie_texture 1") {
                XLOG_ERROR("xtexture header must be: xie_texture 1");
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (StartsWithToken(trimmed, "image")) {
            imageValue = ExtractValueAfterToken(trimmed, "image");
            continue;
        }

        if (StartsWithToken(trimmed, "source")) {
            imageValue = ExtractValueAfterToken(trimmed, "source");
            continue;
        }

        XLOG_ERROR("xtexture only supports lines: image/source/comments");
        return false;
    }

    if (!sawHeader) {
        XLOG_ERROR("xtexture header missing");
        return false;
    }

    if (imageValue.empty()) {
        XLOG_ERROR("xtexture requires: image <relative-or-absolute-path>");
        return false;
    }

    std::filesystem::path imagePath = std::filesystem::path(imageValue);
    if (imagePath.is_relative()) {
        imagePath = descriptorPath.parent_path() / imagePath;
    }

    outImagePath = imagePath.lexically_normal();
    return true;
}

} // namespace Engine
