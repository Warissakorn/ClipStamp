#include "mainwindow.h"
#include "domain.h"
#include "exportqueue.h"
#include "media.h"
#include <QComboBox>
#include <QCalendarWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QLabel>
#include <QDateTimeEdit>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>
using namespace vt;
class UiTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    QString first_, second_;
    QDateTime expected_;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("ClipStamp");
        QCoreApplication::setApplicationName("ClipStampUITests");
        if (findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg not installed");
        first_=dir_.filePath(QStringLiteral("หนึ่ง.mp4")); second_=dir_.filePath(QStringLiteral("สอง.mp4"));
        for(const auto &file : {first_,second_}) {
            QProcess process;
            process.start(findTool("ffmpeg"),{"-v","error","-y","-f","lavfi","-i","color=c=navy:s=320x180:r=24","-t","1.5","-c:v","libx264","-pix_fmt","yuv420p",file});
            QVERIFY(process.waitForStarted(10000)); QVERIFY(process.waitForFinished(15000)); QCOMPARE(process.exitCode(),0);
        }
        expected_=QDateTime(QDate(2025,2,15),QTime(12,34,56),QTimeZone::systemTimeZone());
        QFile file(first_); QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(expected_,QFileDevice::FileModificationTime)); file.close();
    }
    void usesArabicDigitsWithThaiSystemLocale() {
        const auto previous = QLocale();
        QLocale::setDefault(QLocale(QLocale::Thai, QLocale::Thailand));
        MainWindow window;
        window.addPaths({first_});
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("exportButton")->isEnabled(),10000);
        auto date = window.findChild<QDateTimeEdit *>("startDateTime");
        auto elapsed = window.findChild<QSpinBox *>("elapsedStart");
        auto size = window.findChild<QDoubleSpinBox *>("fontSize");
        QVERIFY(date); QVERIFY(elapsed); QVERIFY(size);
        date->setDateTime(QDateTime(QDate(2026,10,23), QTime(12,34,56), QTimeZone::UTC));
        elapsed->setValue(1234567);
        size->setValue(12.5);
        const auto asciiDigits = [](const QString &text) {
            for (auto character : text) if (character.isDigit() && (character < QLatin1Char('0') || character > QLatin1Char('9'))) return false;
            return true;
        };
        QVERIFY(asciiDigits(date->text()));
        QVERIFY(date->text().contains("2026-10-23"));
        QVERIFY(asciiDigits(elapsed->text()));
        QVERIFY(elapsed->text().contains("1234567"));
        QVERIFY(size->text().startsWith("12.50"));
        QCOMPARE(date->calendarWidget()->locale().zeroDigit(), QStringLiteral("0"));
        QVERIFY(asciiDigits(durationLabel(123456789)));
        QVERIFY(qApp->font().pointSizeF() >= 11);
        QVERIFY(qApp->font().family().contains("Noto Sans Thai"));
        QLocale::setDefault(previous);
    }
    void importUsesDateModifiedAndSkipsDuplicates() {
        MainWindow window;
        window.resize(1440, 900);
        window.show();
        const auto screenshotDir = qEnvironmentVariable("CLIPSTAMP_UI_SCREENSHOT_DIR");
        const auto capture = [&](const QString &name) {
            if (!screenshotDir.isEmpty()) {
                QDir().mkpath(screenshotDir);
                QTest::qWait(150);
                QVERIFY(window.grab().save(QDir(screenshotDir).filePath(name)));
            }
        };
        capture("empty.png");
        window.addPaths({first_,second_,first_});
        auto table=window.findChild<QTableWidget *>("clipList");
        auto exportButton=window.findChild<QPushButton *>("exportButton");
        auto start=window.findChild<QDateTimeEdit *>("startDateTime");
        QVERIFY(table); QVERIFY(exportButton); QVERIFY(start);
        QTRY_VERIFY_WITH_TIMEOUT(exportButton->isEnabled(),10000);
        QCOMPARE(table->rowCount(),2); table->selectRow(0);
        QCOMPARE(start->dateTime(),frozenWallClock(expected_));
        auto flow=window.findChild<QComboBox *>("timeFlow");
        QVERIFY(flow); flow->setCurrentIndex(1); table->selectRow(1); QVERIFY(!start->isEnabled());
        table->selectRow(0); QVERIFY(start->isEnabled());
        auto clock=window.findChild<QComboBox *>("clockMode");
        clock->setCurrentIndex(1); QVERIFY(!start->isEnabled());
        clock->setCurrentIndex(0); flow->setCurrentIndex(0);
        capture("loaded.png");
        auto tabs=window.findChild<QTabWidget *>("settingsTabs");
        tabs->setCurrentIndex(1); capture("style.png");
        window.resize(1366,768); capture("compact-style.png");
        tabs->setCurrentIndex(0); capture("compact-time.png");
    }
    void exportButtonCreatesBothFiles() {
        MainWindow window; window.show(); window.addPaths({first_,second_});
        auto exportButton=window.findChild<QPushButton *>("exportButton");
        QTRY_VERIFY_WITH_TIMEOUT(exportButton->isEnabled(),10000);
        window.findChild<QLineEdit *>("outputDirectory")->setText(dir_.filePath("outputs"));
        auto queue=window.findChild<ExportQueue *>();
        QVERIFY(queue); QSignalSpy completed(queue,&ExportQueue::completed);
        QTest::mouseClick(exportButton,Qt::LeftButton);
        QVERIFY(completed.count()>0 || completed.wait(30000));
        QCOMPARE(completed.first()[1].toInt(),0);
        const auto files=completed.first()[0].toStringList(); QCOMPARE(files.size(),2);
        for(const auto &file:files) QVERIFY(QFileInfo(file).size()>1000);
        QVERIFY(exportButton->isEnabled());
    }
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
