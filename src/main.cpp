#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include "ui/main_window.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // If running in an environment without active X11 or Wayland display socket,
    // fallback to offscreen platform to prevent QPA crash
    QString waylandDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
    QString xdgRuntime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    QString waylandSocket = xdgRuntime + "/" + waylandDisplay;
    bool hasWaylandSocket = !waylandDisplay.isEmpty() && QFile::exists(waylandSocket);
    bool hasDisplay = !qEnvironmentVariableIsEmpty("DISPLAY") || hasWaylandSocket;

    if (!hasDisplay) {
        bool hasPlatformArg = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "-platform") {
                hasPlatformArg = true;
                break;
            }
        }
        if (!hasPlatformArg) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            std::cout << "[ai-flow-y2k] No active display server detected: Falling back to offscreen QPA." << std::endl;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("ai-flow-y2k");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("DancinGhost");
    QApplication::setOrganizationDomain("github.com/DancinGhost");

    QCommandLineParser parser;
    parser.setApplicationDescription("ai-flow-y2k: Cross-platform AI Agent Tool & Reasoning Interceptor with Retro Y2K Aesthetics");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port", "Local proxy port to listen on", "port", "8765");
    parser.addOption(portOption);

    QCommandLineOption themeOption(QStringList() << "t" << "theme", "Initial aesthetic theme: vaporwave, matrix, or aero", "theme", "vaporwave");
    parser.addOption(themeOption);

    QCommandLineOption proxyOption(QStringList() << "proxy", "Auto-start proxy listener on launch");
    parser.addOption(proxyOption);

    QCommandLineOption simOption(QStringList() << "simulate", "Auto-run agent simulation flow on launch");
    parser.addOption(simOption);

    parser.process(app);

    std::cout << R"(
    ===============================================================
       ___    ____       ______ __     ____ _       __   __ ____   __ __
      /   |  /  _/      / ____// /    / __ \ |     / /   \ \__ \  / //_/
     / /| |  / / ______/ /_   / /    / / / / | /| / /_____\ \_/ / / ,<   
    / ___ |_/ / /_____/ __/  / /___ / /_/ /| |/ |/ /_____// __/  / /| |  
   /_/  |_/___/      /_/    /_____/ \____/ |__/|__/      /____/ /_/ |_|  
                                                                         
      AI Agent Tool Interceptor, Reasoning Monitor & Human Gatekeeper
      Aesthetics: Vaporwave // Digital Rain // Frutiger Aero
      Architecture: Functional Core, Imperative Shell (FCIS)
    ===============================================================
    )" << std::endl;

    aiflow::ui::MainWindow window;

    if (parser.isSet(themeOption)) {
        QString t = parser.value(themeOption).toLower();
        if (t == "matrix" || t == "digitalrain") {
            window.applyTheme(aiflow::core::ThemeType::DigitalRain);
        } else if (t == "aero" || t == "frutigeraero") {
            window.applyTheme(aiflow::core::ThemeType::FrutigerAero);
        } else {
            window.applyTheme(aiflow::core::ThemeType::Vaporwave);
        }
    }

    if (parser.isSet(proxyOption)) {
        quint16 p = parser.value(portOption).toUShort();
        if (p == 0) p = 8765;
        window.startProxy(p);
    }

    if (parser.isSet(simOption)) {
        window.runSimulation();
    }

    window.show();

    return app.exec();
}
