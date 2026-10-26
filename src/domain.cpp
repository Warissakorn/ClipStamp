#include "domain.h"
#include <QDir>
#include <QFileInfo>
#include <QTimeZone>
#include <algorithm>
namespace vt {
QDateTime frozenWallClock(const QDateTime &instant) {
    const auto local = instant.toLocalTime();
    return QDateTime(local.date(), QTime(local.time().hour(), local.time().minute(), local.time().second()), QTimeZone::UTC);
}
qint64 precedingMs(const Project &p, int index) {
    qint64 result = 0;
    for (int i = 0; i < index && i < p.clips.size(); ++i) result += p.clips[i].durationMs;
    return result;
}
QString durationLabel(qint64 ms) {
    const auto seconds = std::max<qint64>(0, ms) / 1000;
    return QStringLiteral("%1:%2:%3").arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
QString timestamp(const Project &p, int index, qint64 positionMs) {
    if (index < 0 || index >= p.clips.size()) return {};
    const bool continuous = p.flow == TimeFlow::Continuous;
    const auto &base = p.clips[continuous ? 0 : index];
    const qint64 offset = std::max<qint64>(0, positionMs) + (continuous ? precedingMs(p, index) : 0);
    if (p.clock == ClockMode::Elapsed) return durationLabel(base.elapsedStartMs + offset);
    return base.start.addMSecs(offset).toString(p.showDate ? QStringLiteral("yyyy-MM-dd HH:mm:ss") : QStringLiteral("HH:mm:ss"));
}
QSize outputSize(const Project &p, int index, bool sample) {
    if (index < 0 || index >= p.clips.size()) return {1280, 720};
    QSize size = p.clips[(!sample && p.output == OutputMode::Merged) ? 0 : index].size;
    if (size.isEmpty()) size = {1280, 720};
    return {std::max(2, size.width() / 2 * 2), std::max(2, size.height() / 2 * 2)};
}
QString uniqueOutputPath(const QString &dir, const QString &stem) {
    const auto base = stem.isEmpty() ? QStringLiteral("video") : stem;
    QString candidate = QDir(dir).filePath(base + QStringLiteral(".mp4"));
    for (int n = 2; QFileInfo::exists(candidate); ++n)
        candidate = QDir(dir).filePath(base + QStringLiteral("_%1.mp4").arg(n));
    return candidate;
}
}
