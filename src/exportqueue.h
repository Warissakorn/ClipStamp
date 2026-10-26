#pragma once
#include "domain.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <memory>
namespace vt {
class ExportQueue : public QObject {
    Q_OBJECT
public:
    explicit ExportQueue(QObject *parent = nullptr);
    ~ExportQueue() override;
    void start(Project project, QVector<int> indices = {}, int sampleIndex = -1, qint64 samplePosition = 0);
    void cancel();
    bool isRunning() const { return running_; }
signals:
    void jobChanged(int clipIndex, QString state, int percent, QString detail);
    void completed(QStringList files, int failures, bool cancelled);
private:
    Project project_;
    QVector<QVector<int>> jobs_;
    QStringList successes_;
    std::unique_ptr<QTemporaryDir> work_;
    QProcess process_;
    QTimer pumpTimer_;
    QTimer watchdog_;
    QElapsedTimer activity_;
    QByteArray progressBuffer_;
    QByteArray stderr_;
    int job_ = -1;
    int segment_ = 0;
    int failures_ = 0;
    int clipIndex_ = -1;
    qint64 duration_ = 0;
    qint64 samplePosition_ = 0;
    qint64 frame_ = 0;
    qint64 frameCount_ = 0;
    qint64 phaseMs_ = 0;
    qint64 doneMs_ = 0;
    qint64 totalMs_ = 0;
    QSize target_;
    QSize stripSize_;
    bool running_ = false;
    bool cancelled_ = false;
    bool sample_ = false;
    bool concatenating_ = false;
    bool inputClosed_ = false;
    void nextJob();
    void startSegment();
    void startConcat();
    void pump();
    void readProgress();
    void processFinished(int code, QProcess::ExitStatus status);
    void fail(const QString &message);
    void publish();
    void finish();
    QString segmentPath(int segment) const;
};
}
