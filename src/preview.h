#pragma once
#include "domain.h"
#include <QWidget>
#include <QVideoFrame>
namespace vt {
class Preview : public QWidget {
    Q_OBJECT
public:
    explicit Preview(QWidget *parent = nullptr);
    void setFrame(const QVideoFrame &frame);
    void clear();
    void setOverlay(QString text, OverlayStyle style, QSize targetSize);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVideoFrame frame_;
    QString text_;
    OverlayStyle style_;
    QSize targetSize_{1280, 720};
};
}
