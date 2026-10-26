#pragma once
#include <QComboBox>
#include <QStyleOptionComboBox>
#include <QStylePainter>
namespace vt {
// Preserve native popup, keyboard and accessibility behavior; center the selected value.
class CenteredComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent *) override {
        QStylePainter painter(this);
        QStyleOptionComboBox option;
        initStyleOption(&option);
        painter.drawComplexControl(QStyle::CC_ComboBox, option);
        const auto field = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
        const auto text = fontMetrics().elidedText(option.currentText, Qt::ElideRight, qMax(0, field.width() - 8));
        style()->drawItemText(&painter, field.adjusted(4, 0, -4, 0), Qt::AlignCenter,
            option.palette, isEnabled(), text, QPalette::Text);
    }
};
}
