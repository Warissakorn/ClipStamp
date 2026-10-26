#include "overlay.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
namespace vt {
Stamp renderStamp(const QString &text, const OverlayStyle &s, QSize size) {
    QFont font(s.fontFamily);
    font.setPixelSize(std::max(6, int(std::lround(size.height() * s.sizeRatio))));
    QFontMetricsF metrics(font);
    const int margin = std::max(1, int(std::lround(std::min(size.width(), size.height()) * s.marginRatio)));
    const int padding = std::max(2, font.pixelSize() / 5);
    const int border = s.outlineWidth;
    const int available = std::max(1, size.width() - 2 * (margin + padding + border));
    if (metrics.horizontalAdvance(text) > available) {
        font.setPixelSize(std::max(1, int(font.pixelSize() * available / metrics.horizontalAdvance(text))));
        metrics = QFontMetricsF(font);
    }
    QPainterPath glyphs;
    glyphs.addText(0, 0, font, text);
    const QRectF bounds = glyphs.boundingRect();
    const int boxWidth = int(std::ceil(metrics.horizontalAdvance(text))) + 2 * (padding + border);
    const int height = std::min(size.height(), int(std::ceil(metrics.height())) + 2 * (padding + border));
    Stamp stamp;
    stamp.image = QImage(size.width(), height, QImage::Format_RGBA8888);
    stamp.image.fill(Qt::transparent);
    const int anchor = int(s.anchor);
    const int col = anchor % 3;
    const int x = col == 0 ? margin : col == 1 ? (size.width() - boxWidth) / 2 : size.width() - margin - boxWidth;
    stamp.y = anchor < 3 ? margin : size.height() - margin - height;
    stamp.y = std::clamp(stamp.y, 0, std::max(0, size.height() - height));
    QPainter painter(&stamp.image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(QRect(x, 0, boxWidth, height), s.background);
    glyphs.translate(x + padding + border - bounds.left(), padding + border + metrics.ascent());
    if (border > 0) painter.setPen(QPen(s.outline, border * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    else painter.setPen(Qt::NoPen);
    painter.setBrush(s.color);
    painter.drawPath(glyphs);
    // Refill after stroking so a thick outline cannot cover thin glyph interiors.
    painter.fillPath(glyphs, s.color);
    return stamp;
}
}
