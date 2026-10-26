#pragma once
#include "domain.h"
#include <QJsonObject>
namespace vt {
QJsonObject styleToJson(const OverlayStyle &style);
OverlayStyle styleFromJson(const QJsonObject &json);
bool saveProject(const QString &file, const Project &project, QString *error);
bool loadProject(const QString &file, Project *project, QString *error);
bool savePreset(const QString &file, const OverlayStyle &style, QString *error);
bool loadPreset(const QString &file, OverlayStyle *style, QString *error);
}
