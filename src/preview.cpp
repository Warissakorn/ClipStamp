#include "preview.h"
#include "overlay.h"
#include <QPainter>
#include <QPainterPath>
namespace vt {
Preview::Preview(QWidget *parent) : QWidget(parent) {
    setMinimumSize(240, 180);
    setObjectName("videoPreview");
}
void Preview::setFrame(const QVideoFrame &frame) { frame_ = frame; update(); }
void Preview::clear() { frame_ = {}; update(); }
void Preview::setOverlay(QString text, OverlayStyle style, QSize targetSize) {
    text_ = std::move(text); style_ = std::move(style); targetSize_ = targetSize; update();
}
void Preview::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath bounds;
    bounds.addRoundedRect(QRectF(rect()), 8, 8);
    painter.setClipPath(bounds);
    painter.fillRect(rect(), QColor("#131c2b"));
    const QSizeF fitted = QSizeF(targetSize_).scaled(size(), Qt::KeepAspectRatio);
    const QRectF videoRect((width() - fitted.width()) / 2, (height() - fitted.height()) / 2, fitted.width(), fitted.height());
    if (frame_.isValid()) {
        QVideoFrame::PaintOptions options;
        options.backgroundColor = Qt::black;
        options.paintFlags = QVideoFrame::PaintOptions::DontDrawSubtitles;
        frame_.paint(&painter, videoRect, options);
    } else {
        const QPointF center(width() / 2.0, height() / 2.0 - 28);
        painter.setPen(QPen(QColor("#53647c"), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(center.x() - 27, center.y() - 22, 54, 40), 7, 7);
        QPainterPath play;
        play.moveTo(center.x() - 5, center.y() - 12);
        play.lineTo(center.x() + 9, center.y() - 2);
        play.lineTo(center.x() - 5, center.y() + 8);
        play.closeSubpath();
        painter.drawPath(play);
        painter.setPen(QColor("#e0e7f0"));
        auto heading = font(); heading.setPixelSize(17); painter.setFont(heading);
        painter.drawText(QRectF(12, center.y() + 38, width() - 24, 32), Qt::AlignCenter, QStringLiteral("ดูตัวอย่างวิดีโอที่นี่"));
        painter.setPen(QColor("#94a3b8"));
        auto hint = font(); hint.setPixelSize(13); painter.setFont(hint);
        painter.drawText(QRectF(12, center.y() + 72, width() - 24, 28), Qt::AlignCenter, QStringLiteral("เพิ่มวิดีโอ แล้วเลือกคลิปจากรายการ"));
    }
    if (!text_.isEmpty()) {
        const auto stamp = renderStamp(text_, style_, targetSize_);
        painter.save();
        painter.translate(videoRect.topLeft());
        painter.scale(videoRect.width() / targetSize_.width(), videoRect.height() / targetSize_.height());
        painter.drawImage(0, stamp.y, stamp.image);
        painter.restore();
    }
}
}
