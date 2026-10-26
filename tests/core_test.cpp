#include "domain.h"
#include "exportqueue.h"
#include "media.h"
#include "overlay.h"
#include "project.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>
using namespace vt;
class CoreTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir dir_;
    QString first_, second_;
    bool fixtureReady_ = false;
    static bool command(const QString &exe, const QStringList &args, QByteArray *output = nullptr) {
        QProcess p; p.start(exe,args);
        if (!p.waitForStarted(10000) || !p.waitForFinished(60000)) { p.kill(); return false; }
        if (output) *output = p.readAllStandardOutput();
        if (p.exitCode()) qWarning().noquote() << p.readAllStandardError();
        return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    }
    bool fixtures() {
        if (fixtureReady_) return true;
        const auto tool = findTool("ffmpeg"); if (tool.isEmpty()) return false;
        first_ = dir_.filePath(QStringLiteral("คลิป ' [1].mp4")); second_ = dir_.filePath(QStringLiteral("แนวตั้ง.mp4"));
        if (!command(tool,{"-v","error","-y","-f","lavfi","-i","color=c=0x203040:s=320x180:r=24","-f","lavfi","-i","sine=frequency=440:sample_rate=48000","-t","2.4","-c:v","libx264","-pix_fmt","yuv420p","-c:a","aac",first_})) return false;
        if (!command(tool,{"-v","error","-y","-f","lavfi","-i","color=c=0x304020:s=180x320:r=30","-t","1.4","-c:v","libx264","-pix_fmt","yuv420p",second_})) return false;
        fixtureReady_ = true; return true;
    }
    Clip readClip(const QString &file) {
        Clip c; c.path=file; c.modified=QFileInfo(file).lastModified(); c.start=frozenWallClock(c.modified);
        QByteArray json; QString error;
        if (command(findTool("ffprobe"),{"-v","error","-show_streams","-show_format","-of","json",file},&json)) parseProbe(json,&c,&error);
        return c;
    }
    Project fixtureProject() {
        Project p; p.clips={readClip(first_),readClip(second_)}; p.outputDirectory=dir_.path();
        p.clock=ClockMode::Elapsed; p.style.sizeRatio=0.07; return p;
    }
    QImage decode(const QString &file, double seconds, QSize size) {
        QByteArray bytes;
        if (!command(findTool("ffmpeg"),{"-v","error","-ss",QString::number(seconds),"-i",file,"-frames:v","1","-f","rawvideo","-pix_fmt","rgba","pipe:1"},&bytes)) return {};
        if (bytes.size()!=size.width()*size.height()*4) return {};
        return QImage(reinterpret_cast<const uchar *>(bytes.constData()),size.width(),size.height(),QImage::Format_RGBA8888).copy();
    }
    bool matchesStamp(const QImage &actual, const Stamp &stamp, QColor background) {
        if (actual.isNull()) return false;
        QImage expected(actual.size(), QImage::Format_RGBA8888); expected.fill(background);
        { QPainter painter(&expected); painter.drawImage(0,stamp.y,stamp.image); }
        qint64 error=0, count=0;
        for(int y=0;y<stamp.image.height();++y) for(int x=0;x<stamp.image.width();++x) {
            if(qAlpha(stamp.image.pixel(x,y))==0 || expected.pixelColor(x,stamp.y+y).lightness()<140) continue;
            const auto a=actual.pixelColor(x,stamp.y+y), b=expected.pixelColor(x,stamp.y+y);
            error+=std::abs(a.red()-b.red())+std::abs(a.green()-b.green())+std::abs(a.blue()-b.blue()); count+=3;
        }
        return count>20 && double(error)/count<30;
    }
private slots:
    void calendarCrossesMidnight() {
        Project p; Clip c; c.start=QDateTime(QDate(2026,10,23),QTime(23,59,59),QTimeZone::UTC); p.clips={c};
        QCOMPARE(timestamp(p,0,2000),QStringLiteral("2026-10-24 00:00:01"));
        p.showDate=false; QCOMPARE(timestamp(p,0,2000),QStringLiteral("00:00:01"));
    }
    void elapsedDoesNotWrapAt24Hours() {
        Project p; Clip c; c.elapsedStartMs=90000000; p.clips={c}; p.clock=ClockMode::Elapsed;
        QCOMPARE(timestamp(p,0,1000),QStringLiteral("25:00:01"));
    }
    void continuousUsesFirstStartAndFractionalDuration() {
        Project p; Clip a,b; a.durationMs=2400; a.elapsedStartMs=1000; b.elapsedStartMs=99000;
        p.clips={a,b}; p.clock=ClockMode::Elapsed; p.flow=TimeFlow::Continuous;
        QCOMPARE(timestamp(p,1,500),QStringLiteral("00:00:03"));
        QCOMPARE(timestamp(p,1,600),QStringLiteral("00:00:04"));
        p.flow=TimeFlow::PerClip; QCOMPARE(timestamp(p,1,600),QStringLiteral("00:01:39"));
    }
    void wallClockFreezesLocalTimeAndDropsMilliseconds() {
        const auto instant=QDateTime::fromString("2026-10-23T10:20:30.789+07:00",Qt::ISODateWithMs);
        const auto frozen=frozenWallClock(instant);
        QCOMPARE(frozen.date(),instant.toLocalTime().date()); QCOMPARE(frozen.time().msec(),0);
        QCOMPARE(frozen.time().hour(),instant.toLocalTime().time().hour()); QCOMPARE(frozen.offsetFromUtc(),0);
    }
    void projectRoundTripAndMissingSource() {
        Project p; Clip c; c.path=dir_.filePath(QStringLiteral("missing ไทย.mp4")); c.modified=QDateTime::currentDateTime(); c.start=frozenWallClock(c.modified);
        c.elapsedStartMs=123000; p.clips={c}; p.flow=TimeFlow::Continuous; p.output=OutputMode::Merged;
        p.style.background=QColor(1,2,3,40); p.style.fontFamily=QStringLiteral("Leelawadee UI");
        const auto file=dir_.filePath("project.json"); QString error; QVERIFY2(saveProject(file,p,&error),qPrintable(error));
        Project loaded; QVERIFY2(loadProject(file,&loaded,&error),qPrintable(error));
        QCOMPARE(loaded.clips.first().start,p.clips.first().start); QCOMPARE(loaded.clips.first().modified,p.clips.first().modified);
        QCOMPARE(loaded.clips.first().elapsedStartMs,123000); QCOMPARE(loaded.style.background,p.style.background);
        QCOMPARE(loaded.output,OutputMode::Merged); QVERIFY(!loaded.clips.first().ready);
    }
    void malformedProjectDoesNotReplaceState() {
        const auto file=dir_.filePath("bad.json"); QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{broken"); f.close();
        Project p; p.outputDirectory="unchanged"; QString error; QVERIFY(!loadProject(file,&p,&error)); QCOMPARE(p.outputDirectory,QString("unchanged"));
    }
    void presetRoundTrip() {
        OverlayStyle s; s.anchor=Anchor::TopCenter; s.background=QColor(5,6,7,90);
        QString error; const auto file=dir_.filePath("preset.json"); QVERIFY(savePreset(file,s,&error)); OverlayStyle loaded;
        QVERIFY(loadPreset(file,&loaded,&error)); QCOMPARE(loaded.anchor,s.anchor); QCOMPARE(loaded.background,s.background);
    }
    void rendererHasFixedGeometry() {
        OverlayStyle s;
        const auto a=renderStamp("00:00:00",s,{640,360}), b=renderStamp("11:11:11",s,{640,360});
        QCOMPARE(a.image.size(),b.image.size()); QCOMPARE(a.y,b.y);
        for(int i=0;i<6;++i) { s.anchor=Anchor(i); const auto stamp=renderStamp("2026-10-24 12:34:56",s,{180,320}); QVERIFY(stamp.y>=0); QVERIFY(stamp.y+stamp.image.height()<=320); }
    }
    void probeRotationAndInvalidMedia() {
        Clip c; QString error;
        QVERIFY(parseProbe(R"({"format":{"duration":"2"},"streams":[{"codec_type":"video","width":1920,"height":1080,"avg_frame_rate":"30000/1001","side_data_list":[{"rotation":-90}]}]})",&c,&error));
        QCOMPARE(c.size,QSize(1080,1920)); QVERIFY(qAbs(c.fps-29.97003)<0.001);
        QVERIFY(!parseProbe("{}",&c,&error));
    }
    void uniqueNamePreservesExistingFile() {
        const auto file=dir_.filePath("existing.mp4"); QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("original"); f.close();
        QCOMPARE(uniqueOutputPath(dir_.path(),"existing"),dir_.filePath("existing_2.mp4"));
    }
    void importReadsMedia() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        MediaProbe probe; QSignalSpy spy(&probe,&MediaProbe::finished); Clip c; c.path=first_; c.modified=QFileInfo(first_).lastModified(); c.start=frozenWallClock(c.modified);
        probe.probe(c); QVERIFY(spy.wait(15000)); const auto result=qvariant_cast<Clip>(spy.first().first());
        QVERIFY2(result.ready,qPrintable(result.error)); QCOMPARE(result.size,QSize(320,180)); QVERIFY(result.hasAudio); QCOMPARE(result.start,c.start);
    }
    void exportsSeparateWithVisibleMatchingTime() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed); QSignalSpy changes(&queue,&ExportQueue::jobChanged);
        queue.start(p); QVERIFY2(spy.wait(60000),"Export timed out"); const auto result=spy.first();
        QVERIFY2(result[1].toInt()==0,qPrintable([&]{QString all;for(auto row:changes)all+=row[3].toString();return all;}()));
        const auto files=result[0].toStringList(); QCOMPARE(files.size(),2);
        const auto info=readClip(files[0]); QVERIFY(info.ready); QVERIFY(info.hasAudio); QCOMPARE(info.size,QSize(320,180));
        const auto frame=decode(files[0],1.1,info.size); QVERIFY(!frame.isNull());
        QVERIFY(matchesStamp(frame,renderStamp(timestamp(p,0,1100),p.style,info.size),QColor("#203040")));
        QVERIFY(!readClip(files[1]).hasAudio);
    }
    void mergeMixedOrientationAudioAndFrameRates() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); p.output=OutputMode::Merged; p.flow=TimeFlow::Continuous;
        ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed); QSignalSpy changes(&queue,&ExportQueue::jobChanged);
        queue.start(p); QVERIFY(spy.wait(60000)); const auto result=spy.first();
        QVERIFY2(result[1].toInt()==0,qPrintable([&]{QString all;for(auto row:changes)all+=row[3].toString();return all;}()));
        const auto files=result[0].toStringList(); QCOMPARE(files.size(),1); const auto info=readClip(files[0]);
        QCOMPARE(info.size,QSize(320,180)); QVERIFY(info.hasAudio); QVERIFY(qAbs(info.durationMs-p.clips[0].durationMs-p.clips[1].durationMs)<180);
        QVERIFY(qAbs(info.fps-p.clips.first().fps)<0.1);
    }
    void sampleStartsAtSeekPosition() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed);
        queue.start(p,{},0,1100); QVERIFY(spy.wait(60000)); QCOMPARE(spy.first()[1].toInt(),0);
        const auto file=spy.first()[0].toStringList().first(); const auto info=readClip(file);
        QVERIFY(qAbs(info.durationMs-(p.clips[0].durationMs-1100))<100);
        QVERIFY(matchesStamp(decode(file,0.1,info.size),renderStamp(timestamp(p,0,1200),p.style,info.size),QColor("#203040")));
    }
    void continuousExportChangesAtFractionalBoundary() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); p.flow=TimeFlow::Continuous; p.clips[0].elapsedStartMs=59000;
        ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed); queue.start(p);
        QVERIFY(spy.wait(60000)); QCOMPARE(spy.first()[1].toInt(),0);
        const auto file=spy.first()[0].toStringList()[1]; const auto size=readClip(file).size;
        QVERIFY(matchesStamp(decode(file,0.2,size),renderStamp(timestamp(p,1,200),p.style,size),QColor("#304020")));
        QVERIFY(matchesStamp(decode(file,0.8,size),renderStamp(timestamp(p,1,800),p.style,size),QColor("#304020")));
    }
    void calendarExportCrossesMidnight() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); p.clock=ClockMode::Calendar;
        p.clips[0].start=QDateTime(QDate(2026,10,23),QTime(23,59,59),QTimeZone::UTC);
        ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed); queue.start(p,{0});
        QVERIFY(spy.wait(60000)); QCOMPARE(spy.first()[1].toInt(),0);
        const auto file=spy.first()[0].toStringList().first(); const auto size=readClip(file).size;
        QVERIFY(matchesStamp(decode(file,1.2,size),renderStamp(timestamp(p,0,1200),p.style,size),QColor("#203040")));
    }
    void failedClipDoesNotStopNextJob() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); p.clips[0].path=dir_.filePath("deleted.mp4");
        ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed); queue.start(p);
        QVERIFY(spy.wait(60000)); QCOMPARE(spy.first()[1].toInt(),1); QCOMPARE(spy.first()[0].toStringList().size(),1);
    }
    void cancellationCleansTemporaryFiles() {
        if(findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        QVERIFY2(fixtures(),"FFmpeg could not create the test fixtures");
        auto p=fixtureProject(); ExportQueue queue; QSignalSpy spy(&queue,&ExportQueue::completed);
        queue.start(p); queue.cancel(); QVERIFY(spy.count()>0 || spy.wait(10000)); QVERIFY(spy.first()[2].toBool());
        const auto entries=QDir(dir_.path()).entryList({".videotime-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot); QVERIFY(entries.isEmpty());
    }
};
QTEST_MAIN(CoreTest)
#include "core_test.moc"
