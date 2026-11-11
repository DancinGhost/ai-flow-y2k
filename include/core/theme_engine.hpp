#pragma once

#include "core/models.hpp"
#include <string>

namespace aiflow::core {

struct ThemeTokens {
    ThemeType type;
    std::string name;
    std::string subtitle;
    
    // Core palette
    std::string primaryColor;
    std::string secondaryColor;
    std::string accentColor;
    std::string backgroundColor;
    std::string surfaceColor;
    std::string surfaceBorderColor;
    std::string textColor;
    std::string textDimColor;
    std::string headerBgColor;
    std::string cardBgColor;

    // Status colors
    std::string statusApproved;
    std::string statusModified;
    std::string statusRejected;
    std::string statusPending;

    // Font info
    std::string primaryFontFamily;
    std::string monospaceFontFamily;
};

class ThemeEngine {
public:
    // Pure function: get tokens for a theme
    static ThemeTokens getTokens(ThemeType theme);

    // Pure function: generate complete Qt StyleSheet (QSS)
    static std::string generateStyleSheet(ThemeType theme);

    // Pure function: get theme display title with retro glyphs
    static std::string getThemeTitle(ThemeType theme);
};

} // namespace aiflow::core
