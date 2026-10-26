#include "project.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimeZone>
#include <algorithm>
namespace vt {
QJsonObject styleToJson(const OverlayStyle &s) {
    return {{"font", s.fontFamily}, {"size", s.sizeRatio}, {"margin", s.marginRatio},
        {"color", s.color.name(QColor::HexArgb)}, {"background", s.background.name(QColor::HexArgb)},
        {"outline", s.outline.name(QColor::HexArgb)}, {"outlineWidth", s.outlineWidth}, {"anchor", int(s.anchor)}};
}
OverlayStyle styleFromJson(const QJsonObject &o) {
    OverlayStyle s;
    s.fontFamily = o.value("font").toString(s.fontFamily);
    s.sizeRatio = std::clamp(o.value("size").toDouble(s.sizeRatio), 0.01, 0.15);
    s.marginRatio = std::clamp(o.value("margin").toDouble(s.marginRatio), 0.0, 0.2);
    for (const auto &entry : {std::pair{"color", &s.color}, {"background", &s.background}, {"outline", &s.outline}}) {
        const QColor color(o.value(entry.first).toString());
        if (color.isValid()) *entry.second = color;
    }
    s.outlineWidth = std::clamp(o.value("outlineWidth").toInt(s.outlineWidth), 0, 10);
    s.anchor = Anchor(std::clamp(o.value("anchor").toInt(int(s.anchor)), 0, 5));
    return s;
}
static bool writeJson(const QString &file, const QJsonObject &object, QString *error) {
    QSaveFile target(file);
    if (!target.open(QIODevice::WriteOnly) || target.write(QJsonDocument(object).toJson()) < 0 || !target.commit()) {
        if (error) *error = target.errorString();
        return false;
    }
    return true;
}
static bool readJson(const QString &file, QJsonObject *object, QString *error) {
    QFile source(file);
    if (!source.open(QIODevice::ReadOnly)) { if (error) *error = source.errorString(); return false; }
    if (source.size() > 16 * 1024 * 1024) { if (error) *error = QStringLiteral("ไฟล์ใหญ่เกินกำหนด"); return false; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(source.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("รูปแบบ JSON ไม่ถูกต้อง: ") + parse.errorString();
        return false;
    }
    *object = doc.object();
    return true;
}
bool saveProject(const QString &file, const Project &p, QString *error) {
    QJsonArray clips;
    for (const auto &c : p.clips) clips.append(QJsonObject{{"path", c.path},
        {"modified", c.modified.toString(Qt::ISODateWithMs)}, {"start", c.start.toString(Qt::ISODateWithMs)},
        {"elapsedStartMs", double(c.elapsedStartMs)}});
    return writeJson(file, {{"type", "VideoTimeProject"}, {"version", 1}, {"clips", clips},
        {"style", styleToJson(p.style)}, {"clock", int(p.clock)}, {"flow", int(p.flow)},
        {"output", int(p.output)}, {"showDate", p.showDate}, {"outputDirectory", p.outputDirectory}}, error);
}
bool loadProject(const QString &file, Project *project, QString *error) {
    QJsonObject o;
    if (!readJson(file, &o, error)) return false;
    if (o.value("type").toString() != "VideoTimeProject" || o.value("version").toInt() != 1 || !o.value("clips").isArray()) {
        if (error) *error = QStringLiteral("ไม่รองรับไฟล์โปรเจกต์นี้");
        return false;
    }
    Project p;
    p.style = styleFromJson(o.value("style").toObject());
    p.clock = ClockMode(std::clamp(o.value("clock").toInt(), 0, 1));
    p.flow = TimeFlow(std::clamp(o.value("flow").toInt(), 0, 1));
    p.output = OutputMode(std::clamp(o.value("output").toInt(), 0, 1));
    p.showDate = o.value("showDate").toBool(true);
    p.outputDirectory = o.value("outputDirectory").toString();
    for (const auto &value : o.value("clips").toArray()) {
        const auto obj = value.toObject();
        Clip c;
        c.path = obj.value("path").toString();
        if (c.path.isEmpty()) { if (error) *error = QStringLiteral("พบคลิปที่ไม่มีพาธไฟล์"); return false; }
        c.path = QFileInfo(file).dir().absoluteFilePath(c.path);
        c.modified = QDateTime::fromString(obj.value("modified").toString(), Qt::ISODateWithMs);
        const auto parsed = QDateTime::fromString(obj.value("start").toString(), Qt::ISODateWithMs);
        if (!parsed.isValid()) { if (error) *error = QStringLiteral("เวลาเริ่มต้นในโปรเจกต์ไม่ถูกต้อง"); return false; }
        c.start = QDateTime(parsed.date(), parsed.time(), QTimeZone::UTC);
        c.elapsedStartMs = qint64(std::clamp(obj.value("elapsedStartMs").toDouble(), 0.0, 359999999000.0));
        p.clips.append(c);
    }
    *project = p;
    return true;
}
bool savePreset(const QString &file, const OverlayStyle &s, QString *error) {
    return writeJson(file, {{"type", "VideoTimePreset"}, {"version", 1}, {"style", styleToJson(s)}}, error);
}
bool loadPreset(const QString &file, OverlayStyle *style, QString *error) {
    QJsonObject o;
    if (!readJson(file, &o, error)) return false;
    if (o.value("type").toString() != "VideoTimePreset" || o.value("version").toInt() != 1) {
        if (error) *error = QStringLiteral("ไม่รองรับ Preset นี้");
        return false;
    }
    *style = styleFromJson(o.value("style").toObject());
    return true;
}
}
