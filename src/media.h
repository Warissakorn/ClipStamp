#pragma once
#include "domain.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
namespace vt {
QString findTool(const QString &name);
bool parseProbe(const QByteArray &json, Clip *clip, QString *error);
class MediaProbe : public QObject {
    Q_OBJECT
public:
    explicit MediaProbe(QObject *parent = nullptr);
    void probe(Clip clip);
    void stop();
signals:
    void finished(vt::Clip clip);
private:
    QProcess process_;
    QTimer timeout_;
    Clip clip_;
    bool active_ = false;
    void complete(QString error = {});
};
}
Q_DECLARE_METATYPE(vt::Clip)
