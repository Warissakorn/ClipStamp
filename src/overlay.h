#pragma once
#include "domain.h"
#include <QImage>
namespace vt {
struct Stamp { QImage image; int y = 0; };
Stamp renderStamp(const QString &text, const OverlayStyle &style, QSize videoSize);
}
