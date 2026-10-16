#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Engine::UI2D {

// Small, reusable popup model. Rendering and input policy stay with the host
// application; the model provides filtering, keyboard selection and a safe
// selected value for Enum and EntityReference editors.
class SelectionPopup {
public:
    void Open(std::vector<std::string> options, std::string current = {}) {
        m_Options = std::move(options);
        m_Filter.clear();
        m_Selected = std::move(current);
        m_Highlighted = 0;
        ClampHighlight();
        m_Open = true;
    }
    void Close() noexcept { m_Open = false; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_Open; }
    void SetFilter(std::string filter) {
        m_Filter = std::move(filter);
        m_Highlighted = 0;
        ClampHighlight();
    }
    [[nodiscard]] const std::string& Filter() const noexcept { return m_Filter; }
    [[nodiscard]] std::vector<std::string> FilteredOptions() const {
        std::vector<std::string> result;
        for (const auto& option : m_Options) {
            if (m_Filter.empty() || ContainsCaseInsensitive(option, m_Filter)) result.push_back(option);
        }
        return result;
    }
    void MoveHighlight(int delta) {
        const auto options = FilteredOptions();
        if (options.empty()) return;
        const int count = static_cast<int>(options.size());
        m_Highlighted = (m_Highlighted + delta) % count;
        if (m_Highlighted < 0) m_Highlighted += count;
    }
    [[nodiscard]] std::string Commit() {
        const auto options = FilteredOptions();
        if (!options.empty()) m_Selected = options[static_cast<std::size_t>(m_Highlighted)];
        m_Open = false;
        return m_Selected;
    }
    [[nodiscard]] const std::string& Selected() const noexcept { return m_Selected; }
    [[nodiscard]] std::size_t HighlightedIndex() const noexcept { return m_Highlighted; }

private:
    static bool ContainsCaseInsensitive(std::string_view text, std::string_view needle) {
        if (needle.empty()) return true;
        for (std::size_t start = 0; start + needle.size() <= text.size(); ++start) {
            bool equal = true;
            for (std::size_t index = 0; index < needle.size(); ++index) {
                const auto lhs = static_cast<unsigned char>(text[start + index]);
                const auto rhs = static_cast<unsigned char>(needle[index]);
                if (std::tolower(lhs) != std::tolower(rhs)) { equal = false; break; }
            }
            if (equal) return true;
        }
        return false;
    }
    void ClampHighlight() {
        const auto options = FilteredOptions();
        if (options.empty()) m_Highlighted = 0;
        else if (m_Highlighted >= options.size()) m_Highlighted = options.size() - 1;
    }

    bool m_Open = false;
    std::vector<std::string> m_Options;
    std::string m_Filter;
    std::string m_Selected;
    std::size_t m_Highlighted = 0;
};

} // namespace Engine::UI2D
