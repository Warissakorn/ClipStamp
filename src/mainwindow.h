#pragma once
#include "domain.h"
#include "media.h"
#include "exportqueue.h"
#include <QMainWindow>
#include <QSet>
class QTableWidget;
class QTabWidget;
class QComboBox;
class QCheckBox;
class QDateTimeEdit;
class QSpinBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QSlider;
class QMediaPlayer;
class QAudioOutput;
class QVideoSink;
class QTextEdit;
class QCloseEvent;
namespace vt {
class Preview;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void addPaths(const QStringList &paths);
protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void closeEvent(QCloseEvent *) override;
private:
    Project project_;
    MediaProbe probe_;
    ExportQueue queue_;
    QVector<int> pending_;
    QVector<QString> states_;
    QVector<QString> details_;
    QSet<int> failed_;
    int probingIndex_ = -1;
    bool syncing_ = false;
    bool dirty_ = false;
    bool posterPending_ = false;
    QString projectFile_;
    QTableWidget *clips_;
    Preview *preview_;
    QMediaPlayer *player_;
    QAudioOutput *audio_;
    QVideoSink *sink_;
    QSlider *seek_;
    QLabel *playTime_;
    QLabel *sourceInfo_;
    QLabel *summary_;
    QLabel *clipCount_;
    QLabel *previewCaption_;
    QWidget *emptyList_;
    QWidget *calendarFields_;
    QWidget *elapsedFields_;
    QWidget *outputSettingsPanel_;
    QTabWidget *settingsTabs_;
    QComboBox *clock_;
    QComboBox *flow_;
    QComboBox *output_;
    QComboBox *anchor_;
    QCheckBox *showDate_;
    QDateTimeEdit *start_;
    QSpinBox *elapsed_;
    QFontComboBox *font_;
    QDoubleSpinBox *fontSize_;
    QDoubleSpinBox *margin_;
    QSpinBox *outline_;
    QPushButton *textColor_;
    QPushButton *background_;
    QPushButton *outlineColor_;
    QLineEdit *destination_;
    QProgressBar *progress_;
    QPushButton *play_;
    QPushButton *export_;
    QPushButton *sample_;
    QPushButton *cancel_;
    QPushButton *retry_;
    QWidget *settingsPanel_;
    QTextEdit *log_;
    QAction *addAction_;
    QAction *folderAction_;
    QAction *removeAction_;
    QAction *upAction_;
    QAction *downAction_;
    QAction *openAction_;
    QAction *toolsAction_;
    void buildUi();
    void connectUi();
    void rebuildList();
    void selectClip(int row);
    void syncSettings();
    void updateOverlay();
    void updateAvailability();
    void probeNext();
    void moveClip(int delta);
    void removeClip();
    void chooseTools();
    void openProject();
    bool saveCurrent(bool saveAs = false);
    bool maybeSave();
    void runExport(bool sample = false, bool retry = false);
    void markDirty();
    void chooseColor(QColor *color);
};
}
