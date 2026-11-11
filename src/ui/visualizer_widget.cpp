#include "ui/visualizer_widget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <cmath>

namespace aiflow::ui {

VisualizerWidget::VisualizerWidget(QWidget* parent)
    : QWidget(parent), m_animTimer(new QTimer(this)) {
    setFixedHeight(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(m_animTimer, &QTimer::timeout, this, &VisualizerWidget::onAnimationTick);
    m_animTimer->start(33); // ~30 FPS smooth rendering

    initDigitalRain();
    initFrutigerAero();
}

void VisualizerWidget::setTheme(aiflow::core::ThemeType theme) {
    m_theme = theme;
    m_animTime = 0.0f;
    initDigitalRain();
    initFrutigerAero();
    update();
}

void VisualizerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    initDigitalRain();
    initFrutigerAero();
}

void VisualizerWidget::onAnimationTick() {
    m_animTime += 0.04f;
    update();
}

void VisualizerWidget::initDigitalRain() {
    m_rainColumns.clear();
    int colWidth = 14;
    int numCols = width() / colWidth + 1;
    auto* rng = QRandomGenerator::global();

    const QString sampleGlyphs = "0123456789ABCDEFｦｱｳｴｵｶｷｹｺｻｼｽｾｿﾀﾂﾃﾅﾆﾇﾈﾊﾋﾎﾏﾐﾑﾒﾓﾔﾕﾗﾘﾜ";

    for (int i = 0; i < numCols; ++i) {
        RainColumn col;
        col.x = i * colWidth;
        col.y = rng->bounded(height() * 2) - height();
        col.speed = 1.5f + rng->bounded(3.0f);
        col.length = 8 + rng->bounded(12);

        for (int j = 0; j < col.length; ++j) {
            int idx = rng->bounded(sampleGlyphs.size());
            col.glyphs.push_back(sampleGlyphs[idx]);
        }
        m_rainColumns.push_back(col);
    }
}

void VisualizerWidget::initFrutigerAero() {
    m_bubbles.clear();
    int numBubbles = 18;
    auto* rng = QRandomGenerator::global();

    for (int i = 0; i < numBubbles; ++i) {
        AeroBubble b;
        b.x = rng->bounded(width() > 0 ? width() : 800);
        b.y = rng->bounded(height());
        b.radius = 6.0f + rng->bounded(18.0f);
        b.speedY = 0.5f + rng->bounded(1.5f);
        b.wobbleOffset = rng->bounded(6.28f);
        b.opacity = 0.3f + rng->bounded(0.4f);
        m_bubbles.push_back(b);
    }
}

void VisualizerWidget::renderVaporwave(QPainter& painter) {
    int w = width();
    int h = height();

    // 1. Sky Gradient: Deep Indigo to Sunset Fuchsia
    QLinearGradient skyGrad(0, 0, 0, h);
    skyGrad.setColorAt(0.0, QColor(18, 5, 36));
    skyGrad.setColorAt(0.65, QColor(72, 12, 88));
    skyGrad.setColorAt(1.0, QColor(255, 0, 127));
    painter.fillRect(rect(), skyGrad);

    // 2. Glowing Neon Sun with horizontal scanline slits
    painter.save();
    int sunRadius = 42;
    int sunX = w / 2;
    int sunY = h * 0.65;

    QRadialGradient sunGlow(sunX, sunY, sunRadius * 1.5);
    sunGlow.setColorAt(0.0, QColor(255, 230, 0, 240));
    sunGlow.setColorAt(0.6, QColor(255, 0, 128, 200));
    sunGlow.setColorAt(1.0, QColor(255, 0, 128, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(sunGlow);
    painter.drawEllipse(QPoint(sunX, sunY), (int)(sunRadius * 1.5), (int)(sunRadius * 1.5));

    QLinearGradient sunGrad(sunX, sunY - sunRadius, sunX, sunY + sunRadius);
    sunGrad.setColorAt(0.0, QColor(255, 235, 59));
    sunGrad.setColorAt(0.5, QColor(255, 87, 34));
    sunGrad.setColorAt(1.0, QColor(233, 30, 99));
    painter.setBrush(sunGrad);

    // Draw segmented sun slices
    int numSlices = 7;
    for (int i = 0; i < numSlices; ++i) {
        float sliceY = sunY - sunRadius + (i * (2.0f * sunRadius / numSlices));
        float sliceHeight = (2.0f * sunRadius / numSlices) * (1.0f - (i * 0.08f));
        QRectF sliceRect(sunX - sunRadius, sliceY, sunRadius * 2, sliceHeight);
        QPainterPath path;
        path.addEllipse(QPointF(sunX, sunY), sunRadius, sunRadius);
        painter.setClipPath(path);
        painter.drawRect(sliceRect);
    }
    painter.restore();

    // 3. Perspective Neon Wireframe Grid Floor
    float horizonY = h * 0.70f;
    painter.setClipRect(0, (int)horizonY, w, h - (int)horizonY);

    QLinearGradient floorGrad(0, horizonY, 0, h);
    floorGrad.setColorAt(0.0, QColor(20, 0, 40));
    floorGrad.setColorAt(1.0, QColor(8, 0, 18));
    painter.fillRect(0, (int)horizonY, w, h - (int)horizonY, floorGrad);

    // Grid lines (vertical perspective)
    painter.setPen(QPen(QColor(1, 205, 254, 180), 1.5));
    int centerX = w / 2;
    int numPerspLines = 24;
    for (int i = -numPerspLines; i <= numPerspLines; ++i) {
        float spreadBottom = i * (w / 14.0f);
        painter.drawLine(QPointF(centerX + (i * 4.0f), horizonY),
                         QPointF(centerX + spreadBottom, h));
    }

    // Grid lines (horizontal moving towards viewer)
    float gridOffset = std::fmod(m_animTime * 18.0f, 25.0f);
    for (float y = horizonY; y < h; y += 8.0f) {
        float progress = (y - horizonY) / (h - horizonY);
        float lineY = horizonY + std::pow(progress, 1.8f) * (h - horizonY) + (gridOffset * progress * 0.3f);
        if (lineY <= h) {
            painter.setPen(QPen(QColor(255, 0, 127, (int)(80 + progress * 150)), 1.2));
            painter.drawLine(0, (int)lineY, w, (int)lineY);
        }
    }
    painter.setClipping(false);

    // 4. Stylized Retro Title Overlay
    painter.setPen(QColor(255, 255, 255));
    QFont font("Sans-Serif", 13, QFont::Bold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    painter.setFont(font);
    painter.drawText(QRect(20, 16, w - 40, 24), Qt::AlignLeft, "AI・FLOW-Y2K // 仮想世界");

    QFont subFont("Monospace", 9);
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    painter.setFont(subFont);
    painter.setPen(QColor(1, 205, 254));
    painter.drawText(QRect(20, 42, w - 40, 20), Qt::AlignLeft, "[TOOL CALL INTERCEPTOR & REASONING STREAM ENGINE]");
}

void VisualizerWidget::renderDigitalRain(QPainter& painter) {
    int w = width();
    int h = height();

    // 1. Deep Obsidian / Black Background
    painter.fillRect(rect(), QColor(4, 10, 4));

    // 2. Phosphor Glyphs Falling
    QFont font("Monospace", 9, QFont::Bold);
    painter.setFont(font);

    auto* rng = QRandomGenerator::global();

    for (auto& col : m_rainColumns) {
        col.y += col.speed;
        if (col.y - (col.length * 14) > h) {
            col.y = -10;
            col.speed = 1.5f + rng->bounded(3.0f);
        }

        for (int j = 0; j < col.length; ++j) {
            float glyphY = col.y - (j * 13);
            if (glyphY < -10 || glyphY > h) continue;

            if (j == 0) {
                // Leading character: bright white/phosphor highlight
                painter.setPen(QColor(240, 255, 240));
            } else if (j < 3) {
                painter.setPen(QColor(0, 255, 102));
            } else {
                int alpha = std::max(20, 255 - (j * 20));
                painter.setPen(QColor(0, 143, 17, alpha));
            }

            QChar c = col.glyphs[j % col.glyphs.size()];
            painter.drawText(QPointF(col.x, glyphY), QString(c));
        }
    }

    // 3. CRT Scanline Overlay
    painter.setPen(QPen(QColor(0, 0, 0, 45), 1));
    for (int y = 0; y < h; y += 3) {
        painter.drawLine(0, y, w, y);
    }

    // 4. Header Text
    painter.setPen(QColor(0, 255, 102));
    QFont titleFont("Monospace", 12, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRect(20, 16, w - 40, 24), Qt::AlignLeft, "SYSTEM::AI_FLOW_Y2K [PHOSPHOR_MATRIX_V1]");

    QFont subFont("Monospace", 9);
    painter.setFont(subFont);
    painter.setPen(QColor(51, 255, 51));
    painter.drawText(QRect(20, 40, w - 40, 20), Qt::AlignLeft, "STATUS: HOOKED_ON_LLM_WIRE // GATEKEEPER_ACTIVE");
}

void VisualizerWidget::renderFrutigerAero(QPainter& painter) {
    int w = width();
    int h = height();

    // 1. Crystal Aqua Sky Gradient
    QLinearGradient sky(0, 0, 0, h);
    sky.setColorAt(0.0, QColor(0, 120, 215));      // Vista Azure
    sky.setColorAt(0.5, QColor(46, 204, 113));     // Spring Leaf Green
    sky.setColorAt(1.0, QColor(235, 246, 255));    // Clean Sky Tint
    painter.fillRect(rect(), sky);

    // 2. Dynamic Aero Wave Curve with glass gradient
    QPainterPath wave;
    wave.moveTo(0, h * 0.45);
    for (int x = 0; x <= w; x += 15) {
        float waveY = (h * 0.52f) + std::sin((x * 0.015f) + m_animTime) * 12.0f
                                  + std::cos((x * 0.008f) - (m_animTime * 0.7f)) * 8.0f;
        wave.lineTo(x, waveY);
    }
    wave.lineTo(w, h);
    wave.lineTo(0, h);
    wave.closeSubpath();

    QLinearGradient waveGrad(0, h * 0.4f, 0, h);
    waveGrad.setColorAt(0.0, QColor(255, 255, 255, 130));
    waveGrad.setColorAt(0.2, QColor(140, 218, 255, 110));
    waveGrad.setColorAt(1.0, QColor(255, 255, 255, 210));
    painter.fillPath(wave, waveGrad);

    // 3. Floating Skeuomorphic Bubbles with Specular Highlights
    for (auto& b : m_bubbles) {
        b.y -= b.speedY;
        b.x += std::sin(m_animTime + b.wobbleOffset) * 0.5f;

        if (b.y < -b.radius) {
            b.y = h + b.radius;
            b.x = QRandomGenerator::global()->bounded(w > 0 ? w : 800);
        }

        // Outer bubble glow
        QRadialGradient bGrad(b.x - (b.radius * 0.35f), b.y - (b.radius * 0.35f), b.radius);
        bGrad.setColorAt(0.0, QColor(255, 255, 255, (int)(250 * b.opacity)));
        bGrad.setColorAt(0.3, QColor(200, 235, 255, (int)(160 * b.opacity)));
        bGrad.setColorAt(0.85, QColor(100, 190, 255, (int)(90 * b.opacity)));
        bGrad.setColorAt(1.0, QColor(255, 255, 255, (int)(180 * b.opacity)));

        painter.setPen(QPen(QColor(255, 255, 255, (int)(200 * b.opacity)), 1.2));
        painter.setBrush(bGrad);
        painter.drawEllipse(QPointF(b.x, b.y), b.radius, b.radius);

        // Specular gleam dot
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, (int)(240 * b.opacity)));
        painter.drawEllipse(QPointF(b.x - (b.radius * 0.35f), b.y - (b.radius * 0.35f)),
                            b.radius * 0.25f, b.radius * 0.2f);
    }

    // 4. Gloss Sheen across top bar
    QLinearGradient sheen(0, 0, 0, h * 0.45);
    sheen.setColorAt(0.0, QColor(255, 255, 255, 140));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(0, 0, w, (int)(h * 0.45), sheen);

    // 5. Header Typography
    painter.setPen(QColor(10, 45, 80));
    QFont font("Sans-Serif", 13, QFont::Bold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    painter.setFont(font);
    painter.drawText(QRect(20, 16, w - 40, 24), Qt::AlignLeft, "🫧 AI-FLOW-Y2K // AERO GLOSS");

    QFont subFont("Sans-Serif", 9);
    subFont.setBold(true);
    painter.setFont(subFont);
    painter.setPen(QColor(15, 85, 145));
    painter.drawText(QRect(20, 42, w - 40, 20), Qt::AlignLeft, "HUMAN-IN-THE-LOOP AI AGENT OBSERVABILITY & TOOL GATEWAY");
}

void VisualizerWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    switch (m_theme) {
        case aiflow::core::ThemeType::Vaporwave:
            renderVaporwave(painter);
            break;
        case aiflow::core::ThemeType::DigitalRain:
            renderDigitalRain(painter);
            break;
        case aiflow::core::ThemeType::FrutigerAero:
            renderFrutigerAero(painter);
            break;
    }
}

} // namespace aiflow::ui
