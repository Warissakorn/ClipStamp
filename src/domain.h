#pragma once
#include <QColor>
#include <QDateTime>
#include <QSize>
#include <QString>
#include <QVector>
namespace vt {
enum class ClockMode { Calendar, Elapsed };
enum class TimeFlow { PerClip, Continuous };
enum class OutputMode { Separate, Merged };
enum class Anchor { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight };
struct Clip {
    QString path;
    QDateTime modified;
    // Frozen local wall clock represented as UTC, independent of timezone changes.
    QDateTime start;
    qint64 elapsedStartMs = 0;
    qint64 durationMs = 0;
    QSize size;
    double fps = 30.0;
    bool hasAudio = false;
    bool ready = false;
    QString error;
};
struct OverlayStyle {
    QString fontFamily = QStringLiteral("Consolas");
    double sizeRatio = 0.035;
    double marginRatio = 0.025;
    QColor color = Qt::white;
    QColor background = QColor(0, 0, 0, 140);
    QColor outline = Qt::black;
    int outlineWidth = 1;
    Anchor anchor = Anchor::BottomRight;
};
struct Project {
    QVector<Clip> clips;
    OverlayStyle style;
    ClockMode clock = ClockMode::Calendar;
    TimeFlow flow = TimeFlow::PerClip;
    OutputMode output = OutputMode::Separate;
    bool showDate = true;
    QString outputDirectory;
};
QDateTime frozenWallClock(const QDateTime &instant);
qint64 precedingMs(const Project &project, int index);
QString timestamp(const Project &project, int index, qint64 positionMs);
QString durationLabel(qint64 ms);
QSize outputSize(const Project &project, int index, bool sample = false);
QString uniqueOutputPath(const QString &directory, const QString &stem);
}
