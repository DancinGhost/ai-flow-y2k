#pragma once

#include <QWidget>
#include <QTimer>
#include <vector>
#include "core/models.hpp"

namespace aiflow::ui {

class VisualizerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VisualizerWidget(QWidget* parent = nullptr);
    ~VisualizerWidget() override = default;

    void setTheme(aiflow::core::ThemeType theme);
    aiflow::core::ThemeType currentTheme() const { return m_theme; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAnimationTick();

private:
    void renderVaporwave(QPainter& painter);
    void renderDigitalRain(QPainter& painter);
    void renderFrutigerAero(QPainter& painter);

    void initDigitalRain();
    void initFrutigerAero();

    aiflow::core::ThemeType m_theme = aiflow::core::ThemeType::Vaporwave;
    QTimer* m_animTimer = nullptr;
    float m_animTime = 0.0f;

    // Digital rain state
    struct RainColumn {
        int x = 0;
        float y = 0.0f;
        float speed = 1.0f;
        int length = 12;
        std::vector<QChar> glyphs;
    };
    std::vector<RainColumn> m_rainColumns;

    // Frutiger aero bubbles state
    struct AeroBubble {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 10.0f;
        float speedY = 1.0f;
        float wobbleOffset = 0.0f;
        float opacity = 0.5f;
    };
    std::vector<AeroBubble> m_bubbles;
};

} // namespace aiflow::ui
