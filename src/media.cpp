#include "media.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>
namespace vt {
QString findTool(const QString &name) {
    const auto executable = name + QStringLiteral(".exe");
    const auto configured = QSettings().value(name + QStringLiteral("Path")).toString();
    if (QFileInfo(configured).isFile()) return configured;
    // Reuse tool paths saved before the application was renamed.
    const auto legacyConfigured = QSettings(QStringLiteral("VideoTime"), QStringLiteral("VideoTime")).value(name + QStringLiteral("Path")).toString();
    if (QFileInfo(legacyConfigured).isFile()) return legacyConfigured;
    const auto environmentDirectory = qEnvironmentVariable("CLIPSTAMP_FFMPEG_DIR", qEnvironmentVariable("VIDEOTIME_FFMPEG_DIR"));
    if (!environmentDirectory.isEmpty()) {
        const auto file = QDir(environmentDirectory).filePath(executable);
        if (QFileInfo(file).isFile()) return file;
    }
    const auto app = QCoreApplication::applicationDirPath();
    for (const auto &dir : {app + "/tools", app, QStringLiteral("C:/Program Files/Shotcut")}) {
        const auto file = QDir(dir).filePath(executable);
        if (QFileInfo(file).isFile()) return file;
    }
    auto result = QStandardPaths::findExecutable(name);
    return result;
}
static double number(const QJsonValue &value) {
    return value.isString() ? value.toString().toDouble() : value.toDouble();
}
bool parseProbe(const QByteArray &json, Clip *c, QString *error) {
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(json, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) { *error = QStringLiteral("อ่านข้อมูลวิดีโอไม่ได้"); return false; }
    const auto object = doc.object();
    bool found = false;
    double duration = number(object.value("format").toObject().value("duration"));
    for (const auto &value : object.value("streams").toArray()) {
        const auto stream = value.toObject();
        if (stream.value("codec_type").toString() == "audio") c->hasAudio = true;
        if (stream.value("codec_type").toString() != "video" || found || stream.value("disposition").toObject().value("attached_pic").toInt() == 1) continue;
        found = true;
        int width = stream.value("width").toInt();
        int height = stream.value("height").toInt();
        const auto sar = stream.value("sample_aspect_ratio").toString().split(':');
        if (sar.size() == 2 && sar[1].toDouble() > 0 && sar[0].toDouble() > 0)
            width = int(std::lround(width * sar[0].toDouble() / sar[1].toDouble()));
        int rotation = stream.value("tags").toObject().value("rotate").toString().toInt();
        for (const auto &side : stream.value("side_data_list").toArray()) {
            const auto data = side.toObject();
            if (data.contains("rotation")) rotation = int(number(data.value("rotation")));
        }
        if (std::abs(rotation) % 180 == 90) std::swap(width, height);
        c->size = {width, height};
        const auto rate = stream.value("avg_frame_rate").toString().split('/');
        if (rate.size() == 2 && rate[1].toDouble() > 0) c->fps = rate[0].toDouble() / rate[1].toDouble();
        if (!std::isfinite(c->fps) || c->fps < 1 || c->fps > 240) c->fps = 30;
        const double videoDuration = number(stream.value("duration"));
        if (videoDuration > 0) duration = videoDuration;
    }
    if (!found || c->size.isEmpty() || c->size.width() > 8192 || c->size.height() > 8192 || !std::isfinite(duration) || duration <= 0 || duration > 7 * 86400) {
        *error = QStringLiteral("วิดีโอไม่มีระยะเวลาที่อ่านได้ หรือขนาด/ความยาวเกินที่รองรับ (8192px / 7 วัน)");
        return false;
    }
    c->durationMs = qint64(std::lround(duration * 1000));
    c->ready = true;
    c->error.clear();
    return true;
}
MediaProbe::MediaProbe(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] { process_.kill(); complete(QStringLiteral("อ่านข้อมูลไฟล์เกินเวลา 30 วินาที")); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) complete(QStringLiteral("เปิด ffprobe ไม่ได้: ") + process_.errorString());
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (!active_) return;
        QString error;
        if (code != 0 || status != QProcess::NormalExit) error = QString::fromUtf8(process_.readAllStandardError()).right(2000);
        else parseProbe(process_.readAllStandardOutput(), &clip_, &error);
        complete(error);
    });
}
void MediaProbe::probe(Clip clip) {
    clip_ = std::move(clip);
    active_ = true;
    clip_.ready = false;
    clip_.hasAudio = false;
    if (!QFileInfo::exists(clip_.path)) { QTimer::singleShot(0, this, [this] { complete(QStringLiteral("ไม่พบไฟล์ต้นฉบับ")); }); return; }
    const auto tool = findTool("ffprobe");
    if (tool.isEmpty()) { QTimer::singleShot(0, this, [this] { complete(QStringLiteral("ไม่พบ ffprobe กรุณาตั้งค่าเครื่องมือ")); }); return; }
    process_.start(tool, {"-v", "error", "-show_streams", "-show_format", "-of", "json", clip_.path});
    timeout_.start(30000);
}
void MediaProbe::complete(QString error) {
    if (!active_) return;
    active_ = false;
    timeout_.stop();
    if (!error.isEmpty()) { clip_.error = error; clip_.ready = false; }
    emit finished(clip_);
}
void MediaProbe::stop() { active_ = false; timeout_.stop(); process_.kill(); process_.waitForFinished(1000); }
}
