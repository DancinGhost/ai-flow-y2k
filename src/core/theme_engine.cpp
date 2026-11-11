#include "core/theme_engine.hpp"
#include <sstream>

namespace aiflow::core {

ThemeTokens ThemeEngine::getTokens(ThemeType theme) {
    ThemeTokens t;
    t.type = theme;

    switch (theme) {
        case ThemeType::Vaporwave:
            t.name = "Vaporwave Neon";
            t.subtitle = "ＡＩ・ＦＬＯＷ // 思考監視 1989";
            t.primaryColor = "#FF007F";         // Hot Neon Pink
            t.secondaryColor = "#01CDFE";       // Cyber Cyan
            t.accentColor = "#FF71CE";          // Pastel Magenta
            t.backgroundColor = "#120524";      // Deep Night Indigo
            t.surfaceColor = "#1F0B38";          // Royal Plum
            t.surfaceBorderColor = "#FF007F";
            t.textColor = "#FFFFFF";
            t.textDimColor = "#B899D6";
            t.headerBgColor = "#280A4C";
            t.cardBgColor = "#2A0E4E";
            t.statusApproved = "#05FFA1";
            t.statusModified = "#FFE700";
            t.statusRejected = "#FF2A6D";
            t.statusPending = "#01CDFE";
            t.primaryFontFamily = "Sans-Serif";
            t.monospaceFontFamily = "Monospace";
            break;

        case ThemeType::DigitalRain:
            t.name = "Digital Rain";
            t.subtitle = "SYSTEM PHOSPHOR // TERMINAL MATRIX";
            t.primaryColor = "#00FF66";         // Bright Phosphor Green
            t.secondaryColor = "#008F11";       // Deep Matrix Green
            t.accentColor = "#33FF33";          // CRT Highlight
            t.backgroundColor = "#040A04";      // Terminal Obsidian
            t.surfaceColor = "#0A140A";          // Dark Green Carbon
            t.surfaceBorderColor = "#008F11";
            t.textColor = "#44FF44";
            t.textDimColor = "#1D7527";
            t.headerBgColor = "#061206";
            t.cardBgColor = "#0D1E0D";
            t.statusApproved = "#00FF66";
            t.statusModified = "#FFCC00";
            t.statusRejected = "#FF3333";
            t.statusPending = "#00DDFF";
            t.primaryFontFamily = "Monospace";
            t.monospaceFontFamily = "Monospace";
            break;

        case ThemeType::FrutigerAero:
            t.name = "Frutiger Aero";
            t.subtitle = "AERO GLOSS // NATURE-DIGITAL HARMONY 2004";
            t.primaryColor = "#0078D7";         // Clear Azure Blue
            t.secondaryColor = "#2ECC71";       // Spring Leaf Green
            t.accentColor = "#54A0FF";          // Aqua Sky
            t.backgroundColor = "#EBF4FA";      // Fresh Sky White
            t.surfaceColor = "#FFFFFF";          // Pure Glass Surface
            t.surfaceBorderColor = "#B0D4EC";
            t.textColor = "#1E272E";            // Slate Dark
            t.textDimColor = "#576574";
            t.headerBgColor = "#DCEEFB";
            t.cardBgColor = "#F4F9FD";
            t.statusApproved = "#10AC84";
            t.statusModified = "#FF9F43";
            t.statusRejected = "#EE5253";
            t.statusPending = "#2E86DE";
            t.primaryFontFamily = "Sans-Serif";
            t.monospaceFontFamily = "Monospace";
            break;
    }

    return t;
}

std::string ThemeEngine::getThemeTitle(ThemeType theme) {
    switch (theme) {
        case ThemeType::Vaporwave:
            return "🌸 VAPORWAVE // AI-FLOW '95";
        case ThemeType::DigitalRain:
            return "🟢 DIGITAL RAIN // TERMINAL '99";
        case ThemeType::FrutigerAero:
            return "🫧 FRUTIGER AERO // GLASS '04";
    }
    return "AI-FLOW-Y2K";
}

std::string ThemeEngine::generateStyleSheet(ThemeType theme) {
    ThemeTokens t = getTokens(theme);
    std::ostringstream ss;

    if (theme == ThemeType::Vaporwave) {
        ss << R"(
            QMainWindow, QWidget#CentralWidget {
                background-color: )" << t.backgroundColor << R"(;
                color: )" << t.textColor << R"(;
                font-family: ')" << t.primaryFontFamily << R"(';
            }
            QGroupBox {
                border: 2px solid )" << t.surfaceBorderColor << R"(;
                border-radius: 8px;
                margin-top: 1.5ex;
                background-color: )" << t.surfaceColor << R"(;
                color: )" << t.secondaryColor << R"(;
                font-weight: bold;
                font-size: 11pt;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 8px;
                color: )" << t.accentColor << R"(;
            }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FF007F, stop:1 #01CDFE);
                color: #FFFFFF;
                font-weight: bold;
                border: 1px solid #FF71CE;
                border-radius: 6px;
                padding: 7px 14px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FF2A9D, stop:1 #2EE6FF);
                border: 2px solid #FFFFFF;
            }
            QPushButton:pressed {
                background-color: #8A0050;
            }
            QPushButton#DangerBtn {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF0055, stop:1 #990033);
                border: 1px solid #FF557F;
            }
            QPushButton#ApproveBtn {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #05FFA1, stop:1 #01CDFE);
                color: #0A0414;
            }
            QTextEdit, QPlainTextEdit, QTreeWidget, QListWidget, QTableWidget {
                background-color: )" << t.cardBgColor << R"(;
                color: )" << t.textColor << R"(;
                border: 1px solid )" << t.surfaceBorderColor << R"(;
                border-radius: 6px;
                selection-background-color: )" << t.primaryColor << R"(;
                selection-color: #FFFFFF;
            }
            QComboBox {
                background-color: )" << t.surfaceColor << R"(;
                color: )" << t.secondaryColor << R"(;
                border: 1px solid )" << t.primaryColor << R"(;
                border-radius: 4px;
                padding: 4px 8px;
                font-weight: bold;
            }
            QComboBox QAbstractItemView {
                background-color: )" << t.surfaceColor << R"(;
                color: )" << t.textColor << R"(;
                selection-background-color: )" << t.primaryColor << R"(;
            }
            QScrollBar:vertical {
                background: )" << t.backgroundColor << R"(;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: )" << t.primaryColor << R"(;
                min-height: 20px;
                border-radius: 5px;
            }
            QLabel {
                color: )" << t.textColor << R"(;
            }
            QLabel#ThemeSubtitle {
                color: )" << t.secondaryColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
                font-size: 10pt;
                letter-spacing: 2px;
            }
        )";
    } else if (theme == ThemeType::DigitalRain) {
        ss << R"(
            QMainWindow, QWidget#CentralWidget {
                background-color: )" << t.backgroundColor << R"(;
                color: )" << t.textColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
            }
            QGroupBox {
                border: 1px solid )" << t.primaryColor << R"(;
                border-radius: 2px;
                margin-top: 1.5ex;
                background-color: )" << t.surfaceColor << R"(;
                color: )" << t.primaryColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
                font-size: 10pt;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 6px;
                color: #FFFFFF;
                background-color: #008F11;
            }
            QPushButton {
                background-color: #041804;
                color: )" << t.primaryColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
                font-weight: bold;
                border: 1px solid )" << t.primaryColor << R"(;
                border-radius: 2px;
                padding: 6px 12px;
            }
            QPushButton:hover {
                background-color: )" << t.primaryColor << R"(;
                color: #000000;
            }
            QPushButton:pressed {
                background-color: #005511;
                color: #FFFFFF;
            }
            QPushButton#DangerBtn {
                border: 1px solid #FF3333;
                color: #FF3333;
            }
            QPushButton#DangerBtn:hover {
                background-color: #FF3333;
                color: #000000;
            }
            QPushButton#ApproveBtn {
                border: 1px solid #00FF66;
                color: #00FF66;
            }
            QPushButton#ApproveBtn:hover {
                background-color: #00FF66;
                color: #000000;
            }
            QTextEdit, QPlainTextEdit, QTreeWidget, QListWidget, QTableWidget {
                background-color: #020602;
                color: )" << t.textColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
                border: 1px solid )" << t.secondaryColor << R"(;
                selection-background-color: )" << t.secondaryColor << R"(;
                selection-color: #FFFFFF;
            }
            QComboBox {
                background-color: #020602;
                color: )" << t.primaryColor << R"(;
                border: 1px solid )" << t.primaryColor << R"(;
                padding: 4px;
                font-family: ')" << t.monospaceFontFamily << R"(';
            }
            QComboBox QAbstractItemView {
                background-color: #020602;
                color: )" << t.primaryColor << R"(;
                selection-background-color: )" << t.secondaryColor << R"(;
            }
            QScrollBar:vertical {
                background: #020602;
                width: 8px;
            }
            QScrollBar::handle:vertical {
                background: )" << t.primaryColor << R"(;
                border-radius: 1px;
            }
            QLabel#ThemeSubtitle {
                color: )" << t.accentColor << R"(;
                font-family: ')" << t.monospaceFontFamily << R"(';
                letter-spacing: 1px;
            }
        )";
    } else { // Frutiger Aero
        ss << R"(
            QMainWindow, QWidget#CentralWidget {
                background-color: )" << t.backgroundColor << R"(;
                color: )" << t.textColor << R"(;
                font-family: ')" << t.primaryFontFamily << R"(';
            }
            QGroupBox {
                border: 1px solid #B0D4EC;
                border-radius: 10px;
                margin-top: 1.5ex;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFFFFF, stop:1 #F0F7FD);
                color: #006699;
                font-weight: bold;
                font-size: 11pt;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 10px;
                color: #0078D7;
            }
            /* Skeuomorphic glossy aero pill button */
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0.0 #FFFFFF,
                    stop:0.48 #E1EEF9,
                    stop:0.50 #CDE4F7,
                    stop:1.0 #B7D9F5);
                color: #103254;
                font-weight: bold;
                border: 1px solid #7FAFD4;
                border-radius: 14px;
                padding: 6px 16px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0.0 #FFFFFF,
                    stop:0.48 #EBF5FD,
                    stop:0.50 #D8EDFA,
                    stop:1.0 #C2E2FB);
                border: 1px solid #4390CB;
            }
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0.0 #9DCBF0,
                    stop:1.0 #BFE0F7);
            }
            QPushButton#DangerBtn {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0.0 #FFCCCC, stop:0.48 #FFA8A8, stop:0.50 #FF8C8C, stop:1.0 #FF6666);
                border: 1px solid #D63031;
                color: #FFFFFF;
            }
            QPushButton#ApproveBtn {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0.0 #D5F5E3, stop:0.48 #A9DFBF, stop:0.50 #7DCEA0, stop:1.0 #52BE80);
                border: 1px solid #27AE60;
                color: #0E5A2A;
            }
            QTextEdit, QPlainTextEdit, QTreeWidget, QListWidget, QTableWidget {
                background-color: #FFFFFF;
                color: #2D3436;
                border: 1px solid #B2BEC3;
                border-radius: 8px;
                selection-background-color: #74B9FF;
                selection-color: #000000;
            }
            QComboBox {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFFFFF, stop:1 #E4EFF8);
                color: #0984E3;
                border: 1px solid #74B9FF;
                border-radius: 10px;
                padding: 4px 10px;
                font-weight: bold;
            }
            QComboBox QAbstractItemView {
                background-color: #FFFFFF;
                color: #2D3436;
                selection-background-color: #74B9FF;
            }
            QScrollBar:vertical {
                background: #EBF4FA;
                width: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #81C4F8, stop:1 #4FA8ED);
                border-radius: 6px;
                min-height: 25px;
            }
            QLabel#ThemeSubtitle {
                color: #0078D7;
                font-weight: bold;
                letter-spacing: 1px;
            }
        )";
    }

    return ss.str();
}

} // namespace aiflow::core
