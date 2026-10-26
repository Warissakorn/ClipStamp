#include "exportqueue.h"
#include "media.h"
#include "overlay.h"
#include <QDir>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
namespace vt {
ExportQueue::ExportQueue(QObject *parent) : QObject(parent) {
    pumpTimer_.setInterval(5);
    connect(&pumpTimer_, &QTimer::timeout, this, &ExportQueue::pump);
    connect(&process_, &QProcess::started, this, [this] { activity_.start(); pumpTimer_.start(); pump(); });
    connect(&process_, &QProcess::readyReadStandardOutput, this, &ExportQueue::readProgress);
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        stderr_ += process_.readAllStandardError();
        if (stderr_.size() > 16000) stderr_ = stderr_.right(16000);
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ExportQueue::processFinished);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (running_ && error == QProcess::FailedToStart) fail(QStringLiteral("เปิด FFmpeg ไม่ได้: ") + process_.errorString());
    });
    watchdog_.setInterval(1000);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        if (process_.state() == QProcess::Running && activity_.isValid() && activity_.elapsed() > 120000) {
            stderr_ += "\nFFmpeg stopped reporting progress for 120 seconds.";
            process_.kill();
        }
    });
}
ExportQueue::~ExportQueue() { running_ = false; process_.kill(); process_.waitForFinished(2000); }
void ExportQueue::start(Project project, QVector<int> indices, int sampleIndex, qint64 samplePosition) {
    if (running_) return;
    project_ = std::move(project);
    jobs_.clear(); successes_.clear(); job_ = -1; failures_ = 0;
    sample_ = sampleIndex >= 0;
    samplePosition_ = std::max<qint64>(0, samplePosition);
    if (sample_) indices = {sampleIndex};
    else if (indices.isEmpty()) for (int i = 0; i < project_.clips.size(); ++i) indices.append(i);
    if (!sample_ && project_.output == OutputMode::Merged) jobs_.append(indices);
    else for (int index : indices) jobs_.append(QVector<int>{index});
    running_ = true; cancelled_ = false;
    nextJob();
}
void ExportQueue::cancel() {
    if (!running_) return;
    cancelled_ = true;
    pumpTimer_.stop();
    if (process_.state() != QProcess::NotRunning) process_.kill();
    else finish();
}
void ExportQueue::nextJob() {
    if (!running_) return;
    if (cancelled_ || ++job_ >= jobs_.size()) { finish(); return; }
    segment_ = 0; doneMs_ = 0; totalMs_ = 0;
    const auto &indices = jobs_[job_];
    if (indices.isEmpty()) { fail(QStringLiteral("ไม่มีคลิปสำหรับส่งออก")); return; }
    clipIndex_ = indices.first();
    for (int index : indices) {
        if (index < 0 || index >= project_.clips.size() || !project_.clips[index].ready) { fail(QStringLiteral("คลิปยังไม่พร้อมส่งออก")); return; }
        totalMs_ += project_.clips[index].durationMs;
    }
    work_ = std::make_unique<QTemporaryDir>(QDir(project_.outputDirectory).filePath(".videotime-XXXXXX"));
    if (!work_->isValid()) { fail(QStringLiteral("สร้างไฟล์ชั่วคราวในโฟลเดอร์ปลายทางไม่ได้")); return; }
    startSegment();
}
QString ExportQueue::segmentPath(int index) const {
    return QDir(work_->path()).filePath(QStringLiteral("segment-%1.mp4").arg(index));
}
void ExportQueue::startSegment() {
    if (!running_ || cancelled_) { finish(); return; }
    concatenating_ = false; inputClosed_ = false; frame_ = 0;
    stderr_.clear(); progressBuffer_.clear();
    clipIndex_ = jobs_[job_][segment_];
    const auto &clip = project_.clips[clipIndex_];
    const qint64 seek = sample_ ? std::min(samplePosition_, std::max<qint64>(0, clip.durationMs - 1)) : 0;
    duration_ = sample_ ? std::min<qint64>(5000, clip.durationMs - seek) : clip.durationMs;
    if (sample_) totalMs_ = duration_;
    target_ = outputSize(project_, clipIndex_);
    const auto stamp = renderStamp(timestamp(project_, clipIndex_, seek), project_.style, target_);
    stripSize_ = stamp.image.size();
    const bool continuous = project_.flow == TimeFlow::Continuous;
    const auto &base = project_.clips[continuous ? 0 : clipIndex_];
    phaseMs_ = (seek + (continuous ? precedingMs(project_, clipIndex_) : 0)
        + (project_.clock == ClockMode::Elapsed ? base.elapsedStartMs : base.start.time().msec())) % 1000;
    frameCount_ = (duration_ + phaseMs_ + 999) / 1000 + 1;
    const auto ffmpeg = findTool("ffmpeg");
    if (ffmpeg.isEmpty()) { fail(QStringLiteral("ไม่พบ ffmpeg กรุณาตั้งค่าเครื่องมือ")); return; }
    QStringList args{"-hide_banner", "-loglevel", "warning", "-nostdin", "-y", "-filter_complex_threads", "1"};
    if (seek > 0) args << "-ss" << QString::number(seek / 1000.0, 'f', 3);
    args << "-i" << clip.path << "-thread_queue_size" << "4" << "-f" << "rawvideo"
         << "-pixel_format" << "rgba" << "-video_size" << QStringLiteral("%1x%2").arg(stripSize_.width()).arg(stripSize_.height())
         << "-framerate" << "1" << "-probesize" << "32" << "-analyzeduration" << "0" << "-i" << "pipe:0";
    const bool merging = !sample_ && project_.output == OutputMode::Merged;
    if (merging && !clip.hasAudio) args << "-f" << "lavfi" << "-i" << "anullsrc=r=48000:cl=stereo";
    QString filters = QStringLiteral("[0:v:0]setpts=PTS-STARTPTS,scale=%1:%2:force_original_aspect_ratio=decrease:force_divisible_by=2,pad=%1:%2:(ow-iw)/2:(oh-ih)/2,setsar=1")
        .arg(target_.width()).arg(target_.height());
    if (merging) filters += QStringLiteral(",fps=%1").arg(project_.clips.first().fps, 0, 'f', 6);
    filters += QStringLiteral("[base];[1:v]settb=1/1000,setpts=PTS-STARTPTS-%1[stamp];[base][stamp]overlay=x=0:y=%2:eof_action=repeat:repeatlast=1:format=auto,format=yuv420p[v]")
        .arg(phaseMs_).arg(stamp.y);
    args << "-filter_complex" << filters << "-map" << "[v]";
    if (clip.hasAudio) args << "-map" << "0:a:0";
    else if (merging) args << "-map" << "2:a:0";
    if (clip.hasAudio || merging) args << "-af" << "asetpts=PTS-STARTPTS,aresample=48000,apad" << "-c:a" << "aac" << "-b:a" << "192k" << "-ar" << "48000" << "-ac" << "2";
    args << "-c:v" << "libx264" << "-threads" << "4" << "-preset" << "medium" << "-crf" << "20"
         << "-fps_mode" << (merging ? "cfr" : "vfr") << "-t" << QString::number(duration_ / 1000.0, 'f', 3)
         << "-map_metadata" << "-1" << "-metadata:s:v:0" << "rotate=0" << "-movflags" << "+faststart"
         << "-progress" << "pipe:1" << "-nostats" << segmentPath(segment_);
    emit jobChanged(clipIndex_, QStringLiteral("กำลังส่งออก"), int(doneMs_ * 100 / std::max<qint64>(1, totalMs_)), {});
    process_.start(ffmpeg, args);
    activity_.start(); watchdog_.start();
}
void ExportQueue::pump() {
    if (!running_ || cancelled_ || concatenating_ || inputClosed_ || process_.state() != QProcess::Running) return;
    // Bounded backpressure: at most two strips per event-loop turn, never the entire clip in memory.
    for (int n = 0; n < 2 && process_.bytesToWrite() < 4 * 1024 * 1024 && frame_ < frameCount_; ++n) {
        const auto &clip = project_.clips[clipIndex_];
        const qint64 seek = sample_ ? std::min(samplePosition_, std::max<qint64>(0, clip.durationMs - 1)) : 0;
        const auto stamp = renderStamp(timestamp(project_, clipIndex_, seek + std::max<qint64>(0, frame_ * 1000 - phaseMs_)), project_.style, target_);
        QImage image(stripSize_, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        // Fixed raw frame geometry even when glyph shapes change.
        { QPainter painter(&image); painter.drawImage(0, 0, stamp.image); }
        const auto accepted = process_.write(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes());
        if (accepted < 0) return; // Process completion reports the underlying error.
        ++frame_;
    }
    if (frame_ >= frameCount_) { inputClosed_ = true; process_.closeWriteChannel(); pumpTimer_.stop(); }
}
void ExportQueue::readProgress() {
    progressBuffer_ += process_.readAllStandardOutput();
    activity_.restart();
    int newline;
    while ((newline = progressBuffer_.indexOf('\n')) >= 0) {
        const auto line = progressBuffer_.left(newline).trimmed();
        progressBuffer_.remove(0, newline + 1);
        if (line.startsWith("out_time_us=")) {
            const qint64 ms = line.mid(12).toLongLong() / 1000;
            const int percent = concatenating_ ? 99 : int(std::clamp<qint64>((doneMs_ + ms) * 100 / std::max<qint64>(1, totalMs_), 0, 99));
            emit jobChanged(clipIndex_, concatenating_ ? QStringLiteral("กำลังรวมไฟล์") : QStringLiteral("กำลังส่งออก"), percent, {});
        }
    }
}
void ExportQueue::processFinished(int code, QProcess::ExitStatus status) {
    pumpTimer_.stop(); watchdog_.stop();
    if (!running_) return;
    stderr_ += process_.readAllStandardError();
    readProgress();
    if (cancelled_) { emit jobChanged(clipIndex_, QStringLiteral("ยกเลิกแล้ว"), 0, {}); finish(); return; }
    if (code != 0 || status != QProcess::NormalExit) {
        fail(QStringLiteral("FFmpeg ไม่สำเร็จ (%1)\n%2").arg(code).arg(QString::fromUtf8(stderr_).right(6000))); return;
    }
    if (concatenating_) { publish(); return; }
    doneMs_ += duration_;
    if (++segment_ < jobs_[job_].size()) { QTimer::singleShot(0, this, &ExportQueue::startSegment); return; }
    if (!sample_ && project_.output == OutputMode::Merged && jobs_[job_].size() > 1) startConcat();
    else publish();
}
void ExportQueue::startConcat() {
    concatenating_ = true; stderr_.clear(); progressBuffer_.clear();
    QFile list(QDir(work_->path()).filePath("concat.txt"));
    if (!list.open(QIODevice::WriteOnly)) { fail(list.errorString()); return; }
    for (int i = 0; i < jobs_[job_].size(); ++i) list.write(QStringLiteral("file 'segment-%1.mp4'\n").arg(i).toUtf8());
    list.close();
    process_.start(findTool("ffmpeg"), {"-hide_banner", "-loglevel", "warning", "-nostdin", "-y", "-f", "concat", "-safe", "1", "-i", list.fileName(),
        "-map", "0", "-c", "copy", "-movflags", "+faststart", "-progress", "pipe:1", "-nostats", QDir(work_->path()).filePath("result.mp4")});
    activity_.start(); watchdog_.start();
}
void ExportQueue::publish() {
    const bool merged = !sample_ && project_.output == OutputMode::Merged;
    const auto stem = merged ? QStringLiteral("merged_timed") : QFileInfo(project_.clips[clipIndex_].path).completeBaseName() + (sample_ ? "_sample" : "_timed");
    const auto dest = uniqueOutputPath(project_.outputDirectory, stem);
    const auto source = concatenating_ ? QDir(work_->path()).filePath("result.mp4") : segmentPath(0);
    if (!QFile::rename(source, dest)) { fail(QStringLiteral("บันทึกไฟล์ผลลัพธ์ไม่ได้: ") + dest); return; }
    successes_.append(dest);
    for (int index : jobs_[job_]) emit jobChanged(index, QStringLiteral("สำเร็จ"), 100, dest);
    work_.reset();
    QTimer::singleShot(0, this, &ExportQueue::nextJob);
}
void ExportQueue::fail(const QString &message) {
    pumpTimer_.stop(); watchdog_.stop(); ++failures_;
    if (job_ >= 0 && job_ < jobs_.size()) for (int index : jobs_[job_]) emit jobChanged(index, QStringLiteral("ผิดพลาด"), 0, message);
    work_.reset();
    QTimer::singleShot(0, this, &ExportQueue::nextJob);
}
void ExportQueue::finish() {
    if (!running_) return;
    running_ = false; pumpTimer_.stop(); watchdog_.stop(); work_.reset();
    emit completed(successes_, failures_, cancelled_);
}
}
