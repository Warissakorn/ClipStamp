#include "mainwindow.h"
#include "appearance.h"
#include "centeredcombobox.h"
#include <QCalendarWidget>
#include <QFrame>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include "preview.h"
#include "project.h"
#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimeZone>
#include <QToolBar>
#include <QUrl>
#include <QVideoSink>
#include <QVBoxLayout>
#include <algorithm>
namespace vt {
static QPushButton *button(const QString &text, QLayout *layout) {
    auto result = new QPushButton(text);
    layout->addWidget(result);
    return result;
}
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), probe_(this), queue_(this) {
    project_.outputDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath("VideoTime");
    initializeAppearance();
    setLocale(arabicNumberLocale());
    buildUi();
    player_ = new QMediaPlayer(this);
    audio_ = new QAudioOutput(this);
    audio_->setVolume(0.65);
    sink_ = new QVideoSink(this);
    player_->setAudioOutput(audio_);
    player_->setVideoSink(sink_);
    connectUi();
    syncSettings();
    updateAvailability();
    setAcceptDrops(true);
    setMinimumSize(1060, 680);
    const auto available = QGuiApplication::primaryScreen()->availableGeometry();
    resize(std::min(1440, available.width() - 40), std::min(900, available.height() - 60));
    setWindowTitle(QStringLiteral("VideoTime — ใส่เวลาให้วิดีโอ"));
}
MainWindow::~MainWindow() {
    disconnect(player_, nullptr, this, nullptr);
    disconnect(sink_, nullptr, this, nullptr);
    probe_.stop(); queue_.cancel(); player_->stop(); player_->setVideoSink(nullptr);
}
void MainWindow::buildUi() {
    auto label = [](const QString &text, const char *role) {
        auto widget = new QLabel(text);
        widget->setProperty("role", role);
        return widget;
    };
    auto card = [] {
        auto widget = new QFrame;
        widget->setProperty("card", true);
        return widget;
    };
    auto form = [] (QWidget *parent) {
        auto layout = new QFormLayout(parent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setRowWrapPolicy(QFormLayout::WrapAllRows);
        layout->setVerticalSpacing(4);
        layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return layout;
    };
    auto toolButton = [] (QAction *action) {
        auto widget = new QToolButton;
        widget->setDefaultAction(action);
        widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return widget;
    };
    auto body = new QWidget;
    auto rootLayout = new QVBoxLayout(body);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    auto header = new QWidget;
    header->setObjectName("appHeader");
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->setSpacing(10);
    auto identity = new QHBoxLayout;
    identity->setSpacing(12);
    auto brand = new QLabel("VideoTime");
    brand->setObjectName("brand");
    identity->addWidget(brand);
    identity->addWidget(label(QStringLiteral("ใส่เวลาให้วิดีโอ ง่ายในที่เดียว"), "muted"));
    headerLayout->addLayout(identity);
    headerLayout->addStretch();
    addAction_ = new QAction(QStringLiteral("+ เพิ่มวิดีโอ"), this);
    folderAction_ = new QAction(QStringLiteral("เพิ่มโฟลเดอร์"), this);
    openAction_ = new QAction(QStringLiteral("เปิดโปรเจกต์…"), this);
    toolsAction_ = new QAction(QStringLiteral("ตั้งค่าเครื่องมือวิดีโอ…"), this);
    auto saveAction = new QAction(QStringLiteral("บันทึกโปรเจกต์"), this);
    auto saveAs = new QAction(QStringLiteral("บันทึกโปรเจกต์เป็น…"), this);
    saveAction->setShortcut(QKeySequence::Save);
    openAction_->setShortcut(QKeySequence::Open);
    addActions({saveAction, openAction_});
    connect(saveAction, &QAction::triggered, this, [this] { saveCurrent(); });
    connect(saveAs, &QAction::triggered, this, [this] { saveCurrent(true); });
    headerLayout->addWidget(toolButton(folderAction_));
    headerLayout->addWidget(toolButton(addAction_));
    auto projectMenu = new QMenu(this);
    projectMenu->addAction(openAction_);
    projectMenu->addAction(saveAction);
    projectMenu->addAction(saveAs);
    projectMenu->addSeparator();
    projectMenu->addAction(toolsAction_);
    auto projectButton = new QToolButton;
    projectButton->setObjectName("projectMenuButton");
    projectButton->setText(QStringLiteral("โปรเจกต์"));
    projectButton->setMenu(projectMenu);
    projectButton->setPopupMode(QToolButton::InstantPopup);
    projectButton->setProperty("role", "quiet");
    headerLayout->addWidget(projectButton);
    rootLayout->addWidget(header);
    auto content = new QVBoxLayout;
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(10);
    auto splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(10);

    auto left = card();
    left->setMinimumWidth(290);
    left->setMaximumWidth(400);
    auto leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 10, 12, 8);
    leftLayout->setSpacing(8);
    auto listHeading = new QHBoxLayout;
    listHeading->addWidget(label(QStringLiteral("วิดีโอ"), "heading"));
    listHeading->addStretch();
    clipCount_ = label(QStringLiteral("0 คลิป"), "badge");
    clipCount_->setObjectName("clipCount");
    listHeading->addWidget(clipCount_);
    leftLayout->addLayout(listHeading);
    clips_ = new QTableWidget(0, 3);
    clips_->setObjectName("clipList");
    clips_->setHorizontalHeaderLabels({QStringLiteral("ชื่อไฟล์"), QStringLiteral("เวลา"), QStringLiteral("สถานะ")});
    clips_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    clips_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    clips_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    clips_->horizontalHeader()->setMinimumSectionSize(40);
    clips_->setSelectionBehavior(QAbstractItemView::SelectRows);
    clips_->setSelectionMode(QAbstractItemView::SingleSelection);
    clips_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clips_->setWordWrap(false);
    clips_->setTextElideMode(Qt::ElideMiddle);
    clips_->setShowGrid(false);
    clips_->verticalHeader()->hide();
    clips_->verticalHeader()->setDefaultSectionSize(40);
    leftLayout->addWidget(clips_, 1);
    emptyList_ = new QWidget;
    auto emptyLayout = new QVBoxLayout(emptyList_);
    emptyLayout->setContentsMargins(4, 24, 4, 24);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch();
    auto emptyTitle = label(QStringLiteral("ยังไม่มีวิดีโอ"), "heading");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto emptyNote = label(QStringLiteral("ลากไฟล์มาวางที่นี่\nหรือเพิ่มวิดีโอจากปุ่มด้านบน"), "muted");
    emptyNote->setAlignment(Qt::AlignCenter);
    emptyNote->setWordWrap(true);
    emptyLayout->addWidget(emptyNote);
    emptyLayout->addStretch();
    leftLayout->addWidget(emptyList_, 1);
    auto listActions = new QHBoxLayout;
    listActions->setSpacing(4);
    upAction_ = new QAction(QStringLiteral("เลื่อนขึ้น"), this);
    downAction_ = new QAction(QStringLiteral("เลื่อนลง"), this);
    removeAction_ = new QAction(QStringLiteral("นำออก"), this);
    for (auto action : {upAction_, downAction_, removeAction_}) {
        auto actionButton = toolButton(action);
        actionButton->setProperty("role", "quiet");
        listActions->addWidget(actionButton);
    }
    listActions->addStretch();
    leftLayout->addLayout(listActions);

    auto center = card();
    auto middle = new QVBoxLayout(center);
    middle->setContentsMargins(12, 10, 12, 10);
    middle->setSpacing(8);
    auto previewHeading = new QHBoxLayout;
    previewHeading->addWidget(label(QStringLiteral("ตัวอย่างวิดีโอ"), "heading"));
    previewCaption_ = label(QStringLiteral("เลือกคลิปเพื่อดูภาพและเวลา"), "muted");
    previewCaption_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    previewCaption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    previewHeading->addWidget(previewCaption_, 1);
    middle->addLayout(previewHeading);
    preview_ = new Preview;
    middle->addWidget(preview_, 1);
    auto transport = new QHBoxLayout;
    transport->setSpacing(12);
    play_ = button(QStringLiteral("เล่น"), transport);
    play_->setMinimumWidth(62);
    seek_ = new QSlider(Qt::Horizontal);
    seek_->setRange(0, 0);
    seek_->setObjectName("seekSlider");
    transport->addWidget(seek_, 1);
    playTime_ = new QLabel("00:00:00 / 00:00:00");
    playTime_->setObjectName("playTime");
    transport->addWidget(playTime_);
    middle->addLayout(transport);
    sourceInfo_ = label(QStringLiteral("เวลาเริ่มต้นอ่านจาก Date modified ของไฟล์"), "muted");
    sourceInfo_->setObjectName("sourceInfo");
    sourceInfo_->setWordWrap(true);
    middle->addWidget(sourceInfo_);

    auto right = card();
    auto settingsPalette = right->palette();
    settingsPalette.setColor(QPalette::Window, Qt::white);
    right->setPalette(settingsPalette);
    right->setMinimumWidth(338);
    right->setMaximumWidth(370);
    auto rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 10, 12, 10);
    rightLayout->setSpacing(6);
    settingsPanel_ = new QWidget;
    auto settings = new QVBoxLayout(settingsPanel_);
    settings->setContentsMargins(0, 0, 0, 0);
    settingsTabs_ = new QTabWidget;
    settingsTabs_->setObjectName("settingsTabs");
    settingsTabs_->setDocumentMode(true);
    settingsTabs_->tabBar()->setDrawBase(false);
    settings->addWidget(settingsTabs_);
    auto timePage = new QWidget;
    timePage->setObjectName("settingsTimePage");
    timePage->setAttribute(Qt::WA_StyledBackground, true);
    auto timeLayout = new QVBoxLayout(timePage);
    timeLayout->setContentsMargins(0, 4, 4, 0);
    timeLayout->setSpacing(10);
    auto clockFields = new QWidget;
    auto clockForm = form(clockFields);
    clock_ = new CenteredComboBox;
    clock_->setObjectName("clockMode");
    clock_->addItems({QStringLiteral("วันและเวลานาฬิกา"), QStringLiteral("เวลาที่ผ่านไป")});
    clockForm->addRow(label(QStringLiteral("แสดงเวลาแบบ"), "fieldLabel"), clock_);
    timeLayout->addWidget(clockFields);
    calendarFields_ = new QWidget;
    auto calendarForm = form(calendarFields_);
    start_ = new QDateTimeEdit;
    start_->setObjectName("startDateTime");
    start_->setAlignment(Qt::AlignCenter);
    start_->setLocale(arabicNumberLocale());
    start_->setTimeZone(QTimeZone::UTC);
    start_->setDisplayFormat("yyyy-MM-dd  HH:mm:ss");
    start_->setCalendarPopup(true);
    start_->setMinimumDate(QDate(1900, 1, 1));
    start_->setSpecialValueText(QStringLiteral("เลือกคลิปเพื่อกำหนดเวลา"));
    start_->calendarWidget()->setLocale(arabicNumberLocale());
    start_->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendarForm->addRow(label(QStringLiteral("เวลาเริ่มต้นของคลิป"), "fieldLabel"), start_);
    showDate_ = new QCheckBox(QStringLiteral("แสดงวันที่บนวิดีโอ"));
    calendarForm->addRow(showDate_);
    timeLayout->addWidget(calendarFields_);
    elapsedFields_ = new QWidget;
    auto elapsedForm = form(elapsedFields_);
    elapsed_ = new QSpinBox;
    elapsed_->setObjectName("elapsedStart");
    elapsed_->setAlignment(Qt::AlignCenter);
    elapsed_->setLocale(arabicNumberLocale());
    elapsed_->setRange(0, 359999999);
    elapsed_->setGroupSeparatorShown(false);
    elapsed_->setSuffix(QStringLiteral(" วินาที"));
    elapsedForm->addRow(label(QStringLiteral("เริ่มนับจาก"), "fieldLabel"), elapsed_);
    timeLayout->addWidget(elapsedFields_);
    auto flowFields = new QWidget;
    auto flowForm = form(flowFields);
    flow_ = new CenteredComboBox;
    flow_->setObjectName("timeFlow");
    flow_->addItems({QStringLiteral("ใช้เวลาเริ่มของแต่ละคลิป"), QStringLiteral("ต่อเนื่องจากคลิปแรก")});
    flowForm->addRow(label(QStringLiteral("การเดินเวลาหลายคลิป"), "fieldLabel"), flow_);
    timeLayout->addWidget(flowFields);
    auto timeNote = label(QStringLiteral("อ่านเวลาเริ่มจากวันที่แก้ไขไฟล์\nหากเลือกต่อเนื่อง ให้แก้เวลาเริ่มที่คลิปแรก"), "muted");
    timeNote->setWordWrap(true);
    timeLayout->addWidget(timeNote);
    timeLayout->addStretch();
    auto timeScroll = new QScrollArea;
    timeScroll->setWidgetResizable(true);
    timeScroll->setWidget(timePage);
    settingsTabs_->addTab(timeScroll, QStringLiteral("เวลา"));

    auto stylePage = new QWidget;
    stylePage->setObjectName("settingsStylePage");
    stylePage->setAttribute(Qt::WA_StyledBackground, true);
    auto styleLayout = new QVBoxLayout(stylePage);
    styleLayout->setContentsMargins(0, 4, 4, 0);
    styleLayout->setSpacing(8);
    auto styleFields = new QWidget;
    auto styleForm = form(styleFields);
    font_ = new QFontComboBox;
    font_->setEditable(true);
    font_->lineEdit()->setAlignment(Qt::AlignCenter);
    font_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    font_->setMinimumContentsLength(12);
    font_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    styleForm->addRow(label(QStringLiteral("ฟอนต์ตัวเลข"), "fieldLabel"), font_);
    anchor_ = new CenteredComboBox;
    anchor_->addItems({QStringLiteral("ซ้ายบน"), QStringLiteral("กลางบน"), QStringLiteral("ขวาบน"), QStringLiteral("ซ้ายล่าง"), QStringLiteral("กลางล่าง"), QStringLiteral("ขวาล่าง")});
    styleForm->addRow(label(QStringLiteral("ตำแหน่ง"), "fieldLabel"), anchor_);
    styleLayout->addWidget(styleFields);
    auto dimensionRow = new QHBoxLayout;
    dimensionRow->setSpacing(12);
    auto sizeFields = new QWidget;
    auto sizeForm = form(sizeFields);
    fontSize_ = new QDoubleSpinBox;
    fontSize_->setObjectName("fontSize");
    fontSize_->setAlignment(Qt::AlignCenter);
    fontSize_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    fontSize_->setLocale(arabicNumberLocale());
    fontSize_->setGroupSeparatorShown(false);
    fontSize_->setRange(1, 15);
    fontSize_->setDecimals(2);
    fontSize_->setSingleStep(0.25);
    fontSize_->setSuffix(" %");
    sizeForm->addRow(label(QStringLiteral("ขนาดตัวเลข"), "fieldLabel"), fontSize_);
    dimensionRow->addWidget(sizeFields);
    auto marginFields = new QWidget;
    auto marginForm = form(marginFields);
    margin_ = new QDoubleSpinBox;
    margin_->setObjectName("overlayMargin");
    margin_->setAlignment(Qt::AlignCenter);
    margin_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    margin_->setLocale(arabicNumberLocale());
    margin_->setGroupSeparatorShown(false);
    margin_->setRange(0, 20);
    margin_->setDecimals(1);
    margin_->setSingleStep(0.5);
    margin_->setSuffix(" %");
    marginForm->addRow(label(QStringLiteral("ระยะจากขอบ"), "fieldLabel"), margin_);
    dimensionRow->addWidget(marginFields);
    styleLayout->addLayout(dimensionRow);
    textColor_ = new QPushButton(QStringLiteral("ตัวเลข"));
    background_ = new QPushButton(QStringLiteral("พื้นหลัง"));
    outlineColor_ = new QPushButton(QStringLiteral("ขอบ"));
    auto colorLabel = label(QStringLiteral("สีและความโปร่งใส"), "fieldLabel");
    styleLayout->addWidget(colorLabel);
    auto colorRow = new QHBoxLayout;
    colorRow->setSpacing(6);
    for (auto colorButton : {textColor_, background_, outlineColor_}) {
        colorButton->setIconSize({20, 20});
        colorButton->setToolTip(QStringLiteral("เลือกสีและปรับความโปร่งใส"));
        colorButton->setStyleSheet("padding: 2px 6px; font-size: 13px;");
        colorRow->addWidget(colorButton, 1);
    }
    styleLayout->addLayout(colorRow);
    auto borderFields = new QWidget;
    auto borderForm = form(borderFields);
    outline_ = new QSpinBox;
    outline_->setObjectName("outlineWidth");
    outline_->setAlignment(Qt::AlignCenter);
    outline_->setLocale(arabicNumberLocale());
    outline_->setRange(0, 10);
    outline_->setSuffix(" px");
    borderForm->addRow(label(QStringLiteral("ความหนาเส้นขอบ"), "fieldLabel"), outline_);
    styleLayout->addWidget(borderFields);
    auto presetRow = new QHBoxLayout;
    presetRow->setSpacing(8);
    auto loadPresetButton = button(QStringLiteral("เปิดรูปแบบ"), presetRow);
    auto savePresetButton = button(QStringLiteral("บันทึกรูปแบบ"), presetRow);
    styleLayout->addLayout(presetRow);
    styleLayout->addStretch();
    connect(loadPresetButton, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("เปิดรูปแบบ"), {}, "VideoTime Preset (*.json)");
        if (file.isEmpty()) return;
        QString error;
        if (!loadPreset(file, &project_.style, &error)) { QMessageBox::warning(this, QStringLiteral("เปิดรูปแบบไม่ได้"), error); return; }
        syncSettings(); updateOverlay(); markDirty();
    });
    connect(savePresetButton, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getSaveFileName(this, QStringLiteral("บันทึกรูปแบบ"), "style.json", "VideoTime Preset (*.json)");
        if (file.isEmpty()) return;
        QString error;
        if (!savePreset(file, project_.style, &error)) QMessageBox::warning(this, QStringLiteral("บันทึกไม่ได้"), error);
    });
    auto styleScroll = new QScrollArea;
    styleScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    styleScroll->setWidgetResizable(true);
    styleScroll->setWidget(stylePage);
    settingsTabs_->addTab(styleScroll, QStringLiteral("รูปแบบ"));
    settingsTabs_->setTabToolTip(1, QStringLiteral("รูปแบบนี้ใช้กับวิดีโอทุกคลิป"));
    rightLayout->addWidget(settingsPanel_, 1);
    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({310, 720, 342});
    content->addWidget(splitter, 1);

    auto exportCard = card();
    auto exportLayout = new QVBoxLayout(exportCard);
    exportLayout->setContentsMargins(12, 10, 12, 10);
    exportLayout->setSpacing(8);
    outputSettingsPanel_ = new QWidget;
    auto destinationRow = new QHBoxLayout(outputSettingsPanel_);
    destinationRow->setContentsMargins(0, 0, 0, 0);
    destinationRow->setSpacing(10);
    output_ = new CenteredComboBox;
    output_->setObjectName("outputMode");
    output_->addItems({QStringLiteral("แยกไฟล์ตามคลิป"), QStringLiteral("รวมเป็นไฟล์เดียว")});
    output_->setMinimumWidth(190);
    output_->setToolTip(QStringLiteral("ไฟล์ MP4 · รวมคลิปโดยใช้ขนาดและ FPS ของคลิปแรก"));
    destinationRow->addWidget(output_);
    destinationRow->addSpacing(8);
    destinationRow->addWidget(label(QStringLiteral("บันทึกที่"), "fieldLabel"));
    destination_ = new QLineEdit;
    destination_->setObjectName("outputDirectory");
    destination_->setAlignment(Qt::AlignCenter);
    destination_->setPlaceholderText(QStringLiteral("เลือกโฟลเดอร์ปลายทาง"));
    destinationRow->addWidget(destination_, 1);
    auto browse = button(QStringLiteral("เลือกโฟลเดอร์"), destinationRow);
    auto reveal = button(QStringLiteral("เปิดโฟลเดอร์"), destinationRow);
    reveal->setProperty("role", "quiet");
    exportLayout->addWidget(outputSettingsPanel_);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto dir = QFileDialog::getExistingDirectory(this, QStringLiteral("โฟลเดอร์ปลายทาง"), destination_->text());
        if (!dir.isEmpty()) destination_->setText(dir);
    });
    connect(reveal, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(destination_->text())); });
    auto footer = new QHBoxLayout;
    footer->setSpacing(10);
    summary_ = label(QStringLiteral("เพิ่มวิดีโอเพื่อเริ่มต้น"), "muted");
    summary_->setObjectName("jobSummary");
    summary_->setWordWrap(true);
    footer->addWidget(summary_, 1);
    retry_ = button(QStringLiteral("ลองใหม่"), footer);
    cancel_ = button(QStringLiteral("ยกเลิกงาน"), footer);
    sample_ = button(QStringLiteral("ตัวอย่าง 5 วินาที"), footer);
    export_ = button(QStringLiteral("ส่งออกทั้งหมด"), footer);
    export_->setObjectName("exportButton");
    exportLayout->addLayout(footer);
    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    exportLayout->addWidget(progress_);
    content->addWidget(exportCard);
    rootLayout->addLayout(content, 1);
    setCentralWidget(body);
    auto dock = new QDockWidget(QStringLiteral("รายละเอียดงาน"), this);
    log_ = new QTextEdit;
    log_->setReadOnly(true);
    dock->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->hide();
    projectMenu->addSeparator();
    projectMenu->addAction(dock->toggleViewAction());
}
void MainWindow::connectUi() {
    connect(addAction_, &QAction::triggered, this, [this] {
        addPaths(QFileDialog::getOpenFileNames(this, QStringLiteral("เพิ่มวิดีโอ"), {}, QStringLiteral("วิดีโอ (*.mp4 *.mov *.mkv *.avi *.mts *.m2ts *.webm *.m4v *.wmv);;ไฟล์ทั้งหมด (*)")));
    });
    connect(folderAction_, &QAction::triggered, this, [this] {
        const auto dir = QFileDialog::getExistingDirectory(this, QStringLiteral("เพิ่มวิดีโอจากโฟลเดอร์ รวมโฟลเดอร์ย่อย"));
        if (!dir.isEmpty()) addPaths({dir});
    });
    connect(openAction_, &QAction::triggered, this, &MainWindow::openProject);
    connect(toolsAction_, &QAction::triggered, this, &MainWindow::chooseTools);
    connect(upAction_, &QAction::triggered, this, [this] { moveClip(-1); });
    connect(downAction_, &QAction::triggered, this, [this] { moveClip(1); });
    connect(removeAction_, &QAction::triggered, this, &MainWindow::removeClip);
    connect(clips_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) { if (!syncing_) selectClip(row); });
    connect(&probe_, &MediaProbe::finished, this, [this](Clip clip) {
        if (probingIndex_ >= 0 && probingIndex_ < project_.clips.size()) {
            project_.clips[probingIndex_] = clip;
            states_[probingIndex_] = clip.ready ? QStringLiteral("พร้อม") : QStringLiteral("อ่านไม่ได้");
            details_[probingIndex_] = clip.error;
            if (!clip.ready) log_->append(clip.path + "\n" + clip.error.toHtmlEscaped());
        }
        probingIndex_ = -1; rebuildList(); updateOverlay();
        if (clips_->currentRow() >= 0) selectClip(clips_->currentRow());
        QTimer::singleShot(0,this,&MainWindow::probeNext);
    });
    connect(sink_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        preview_->setFrame(frame);
        if (posterPending_ && frame.isValid()) { posterPending_ = false; player_->pause(); }
        updateOverlay();
    });
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus state) {
        if (state == QMediaPlayer::LoadedMedia && posterPending_) player_->play();
    });
    connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (!seek_->isSliderDown()) seek_->setValue(int(position));
        const int row = clips_->currentRow();
        const qint64 total = row >= 0 && row < project_.clips.size() ? project_.clips[row].durationMs : 0;
        playTime_->setText(durationLabel(position) + " / " + durationLabel(total)); updateOverlay();
    });
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        play_->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("หยุดชั่วคราว") : QStringLiteral("เล่น"));
    });
    connect(player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) {
        summary_->setText(QStringLiteral("Preview เปิดไม่ได้: ") + error); log_->append(error.toHtmlEscaped());
    });
    connect(play_, &QPushButton::clicked, this, [this] {
        posterPending_ = false;
        if (player_->playbackState() == QMediaPlayer::PlayingState) player_->pause(); else player_->play();
    });
    connect(seek_, &QSlider::sliderMoved, this, [this](int value) { posterPending_ = false; player_->setPosition(value); });
    auto configChanged = [this] {
        if (syncing_) return;
        project_.clock = ClockMode(clock_->currentIndex()); project_.flow = TimeFlow(flow_->currentIndex());
        project_.output = OutputMode(output_->currentIndex()); project_.showDate = showDate_->isChecked();
        project_.style.fontFamily = font_->currentFont().family(); project_.style.sizeRatio = fontSize_->value() / 100;
        project_.style.marginRatio = margin_->value() / 100; project_.style.anchor = Anchor(anchor_->currentIndex());
        project_.style.outlineWidth = outline_->value();
        updateOverlay(); updateAvailability(); markDirty();
    };
    for (auto combo : {clock_, flow_, output_, anchor_}) connect(combo, &QComboBox::currentIndexChanged, this, configChanged);
    connect(showDate_, &QCheckBox::toggled, this, configChanged);
    connect(font_, &QFontComboBox::currentFontChanged, this, configChanged);
    connect(fontSize_, &QDoubleSpinBox::valueChanged, this, configChanged);
    connect(margin_, &QDoubleSpinBox::valueChanged, this, configChanged);
    connect(outline_, &QSpinBox::valueChanged, this, configChanged);
    connect(start_, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime &value) {
        const int row = clips_->currentRow();
        if (!syncing_ && row >= 0) { project_.clips[row].start = value; updateOverlay(); markDirty(); }
    });
    connect(elapsed_, &QSpinBox::valueChanged, this, [this](int seconds) {
        const int row = clips_->currentRow();
        if (!syncing_ && row >= 0) { project_.clips[row].elapsedStartMs = qint64(seconds) * 1000; updateOverlay(); markDirty(); }
    });
    connect(textColor_, &QPushButton::clicked, this, [this] { chooseColor(&project_.style.color); });
    connect(background_, &QPushButton::clicked, this, [this] { chooseColor(&project_.style.background); });
    connect(outlineColor_, &QPushButton::clicked, this, [this] { chooseColor(&project_.style.outline); });
    connect(destination_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!syncing_) { project_.outputDirectory = text; markDirty(); }
    });
    connect(export_, &QPushButton::clicked, this, [this] { runExport(); });
    connect(sample_, &QPushButton::clicked, this, [this] { runExport(true); });
    connect(retry_, &QPushButton::clicked, this, [this] { runExport(false,true); });
    connect(cancel_, &QPushButton::clicked, &queue_, &ExportQueue::cancel);
    connect(&queue_, &ExportQueue::jobChanged, this, [this](int index, const QString &state, int percent, const QString &detail) {
        if (index >= 0 && index < states_.size()) {
            states_[index] = state; details_[index] = detail;
            if (state == QStringLiteral("ผิดพลาด")) failed_.insert(index);
            if (state == QStringLiteral("สำเร็จ")) failed_.remove(index);
        }
        progress_->setValue(percent);
        summary_->setText(QStringLiteral("%1 · %2 · %3%").arg(index >= 0 ? QFileInfo(project_.clips[index].path).fileName() : QString(), state).arg(percent));
        if (!detail.isEmpty()) log_->append(detail.toHtmlEscaped());
        rebuildList();
    });
    connect(&queue_, &ExportQueue::completed, this, [this](const QStringList &files, int failures, bool cancelled) {
        progress_->setValue(cancelled || failures ? 0 : 100);
        summary_->setText(QStringLiteral("%1 · สำเร็จ %2 ไฟล์ · ผิดพลาด %3 งาน").arg(cancelled ? QStringLiteral("ยกเลิกแล้ว") : QStringLiteral("เสร็จสิ้น")).arg(files.size()).arg(failures));
        updateAvailability();
    });
}
void MainWindow::chooseColor(QColor *color) {
    const auto chosen = QColorDialog::getColor(*color,this,QStringLiteral("เลือกสีและความโปร่งใส"),QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) { *color = chosen; syncSettings(); updateOverlay(); markDirty(); }
}
void MainWindow::markDirty() {
    dirty_ = true;
    setWindowTitle(QStringLiteral("VideoTime — %1 *").arg(projectFile_.isEmpty() ? QStringLiteral("โปรเจกต์ใหม่") : QFileInfo(projectFile_).fileName()));
}
void MainWindow::syncSettings() {
    syncing_ = true;
    clock_->setCurrentIndex(int(project_.clock)); flow_->setCurrentIndex(int(project_.flow)); output_->setCurrentIndex(int(project_.output));
    showDate_->setChecked(project_.showDate); font_->setCurrentFont(QFont(project_.style.fontFamily));
    fontSize_->setValue(project_.style.sizeRatio * 100); margin_->setValue(project_.style.marginRatio * 100);
    anchor_->setCurrentIndex(int(project_.style.anchor)); outline_->setValue(project_.style.outlineWidth);
    destination_->setText(project_.outputDirectory);
    const auto swatch = [](QColor color) {
        QPixmap image(20, 20);
        image.fill(Qt::white);
        QPainter painter(&image);
        for (int y = 0; y < 20; y += 5) for (int x = 0; x < 20; x += 5)
            if ((x / 5 + y / 5) % 2 == 0) painter.fillRect(x, y, 5, 5, QColor("#e5e9ef"));
        painter.fillRect(image.rect(), color);
        painter.setPen(QColor("#cbd3df"));
        painter.drawRect(0, 0, 19, 19);
        return QIcon(image);
    };
    textColor_->setIcon(swatch(project_.style.color));
    background_->setIcon(swatch(project_.style.background));
    outlineColor_->setIcon(swatch(project_.style.outline));
    syncing_ = false;
}
void MainWindow::rebuildList() {
    const int current = clips_->currentRow();
    syncing_ = true;
    clipCount_->setText(QStringLiteral("%1 คลิป").arg(project_.clips.size()));
    clips_->setRowCount(int(project_.clips.size()));
    for (int i = 0; i < project_.clips.size(); ++i) {
        const auto &c = project_.clips[i];
        const QStringList values{QFileInfo(c.path).fileName(), c.ready ? durationLabel(c.durationMs) : QStringLiteral("—"), states_.value(i)};
        for (int col = 0; col < 3; ++col) {
            auto item = clips_->item(i,col);
            if (!item) { item = new QTableWidgetItem; clips_->setItem(i,col,item); }
            item->setText(values[col]); item->setToolTip(c.path + "\n" + details_.value(i));
            if (col == 1) { item->setFont(QFont("Consolas", 10)); item->setTextAlignment(Qt::AlignCenter); }
            if (col == 2) {
                item->setFont(QFont(qApp->font().family(), 10));
                item->setForeground(QColor(c.error.isEmpty() ? "#667085" : "#b42318"));
            }
        }
    }
    if (current >= 0 && current < clips_->rowCount()) clips_->selectRow(current);
    syncing_ = false;
    updateAvailability();
}
void MainWindow::addPaths(const QStringList &paths) {
    if (queue_.isRunning() || probingIndex_ >= 0 || !pending_.isEmpty()) return;
    QStringList files;
    const QStringList extensions{"mp4","mov","mkv","avi","mts","m2ts","webm","m4v","wmv"};
    for (const auto &entry : paths) {
        if (QFileInfo(entry).isDir()) {
            QDirIterator it(entry,QDir::Files,QDirIterator::Subdirectories);
            QStringList found;
            while (it.hasNext()) { const auto file = it.next(); if (extensions.contains(QFileInfo(file).suffix().toLower())) found.append(file); }
            found.sort(Qt::CaseInsensitive); files.append(found);
        } else files.append(entry);
    }
    for (const auto &file : files) {
        const QFileInfo info(file);
        const auto absolute = info.absoluteFilePath();
        bool duplicate = false;
        for (const auto &c : project_.clips) if (c.path.compare(absolute,Qt::CaseInsensitive) == 0) { duplicate = true; break; }
        if (duplicate) continue;
        Clip c; c.path = absolute; c.modified = info.lastModified();
        c.start = frozenWallClock(c.modified.isValid() ? c.modified : QDateTime::currentDateTime());
        project_.clips.append(c); states_.append(QStringLiteral("รออ่านข้อมูล")); details_.append(QString()); pending_.append(int(project_.clips.size()-1));
    }
    if (files.isEmpty()) return;
    markDirty(); rebuildList();
    if (clips_->currentRow() < 0 && !project_.clips.isEmpty()) clips_->selectRow(0);
    probeNext();
}
void MainWindow::probeNext() {
    if (probingIndex_ >= 0) return;
    if (pending_.isEmpty()) {
        summary_->setText(QStringLiteral("อ่านข้อมูลเสร็จแล้ว · %1 คลิป").arg(project_.clips.size()));
        updateAvailability(); return;
    }
    probingIndex_ = pending_.takeFirst();
    states_[probingIndex_] = QStringLiteral("กำลังอ่าน"); rebuildList();
    summary_->setText(QStringLiteral("กำลังอ่านข้อมูล · %1").arg(QFileInfo(project_.clips[probingIndex_].path).fileName()));
    probe_.probe(project_.clips[probingIndex_]);
}
void MainWindow::selectClip(int row) {
    if (row < 0 || row >= project_.clips.size()) {
        previewCaption_->setText(QStringLiteral("เลือกคลิปเพื่อดูภาพและเวลา"));
        sourceInfo_->setText(QStringLiteral("เวลาเริ่มต้นอ่านจาก Date modified ของไฟล์"));
        playTime_->setText("00:00:00 / 00:00:00");
        player_->stop(); player_->setSource({}); preview_->clear(); updateOverlay(); updateAvailability(); return; }
    const auto &c = project_.clips[row];
    previewCaption_->setText(QFileInfo(c.path).fileName());
    syncing_ = true; start_->setDateTime(c.start); elapsed_->setValue(int(c.elapsedStartMs / 1000)); syncing_ = false;
    sourceInfo_->setText(QStringLiteral("Date modified  %1  ·  %2 × %3  ·  %4 FPS  ·  %5").arg(c.modified.toString("yyyy-MM-dd HH:mm:ss"))
        .arg(c.size.width()).arg(c.size.height()).arg(c.fps,0,'f',2).arg(c.hasAudio ? QStringLiteral("มีเสียง") : QStringLiteral("ไม่มีเสียง")));
    const QUrl source = QUrl::fromLocalFile(c.path);
    if (player_->source() != source && c.ready) {
        player_->stop(); preview_->clear(); posterPending_ = true; player_->setSource(source); seek_->setValue(0);
    }
    if (!c.ready) { player_->stop(); player_->setSource({}); preview_->clear(); }
    seek_->setRange(0,int(std::min<qint64>(c.durationMs,2147483647)));
    playTime_->setText(durationLabel(player_->position()) + " / " + durationLabel(c.durationMs));
    updateOverlay(); updateAvailability();
}
void MainWindow::updateOverlay() {
    const int row = clips_->currentRow();
    if (row < 0 || row >= project_.clips.size() || !project_.clips[row].ready) preview_->setOverlay({}, project_.style, {1280,720});
    else preview_->setOverlay(timestamp(project_,row,player_->position()), project_.style, outputSize(project_,row));
}
void MainWindow::updateAvailability() {
    const bool importing = probingIndex_ >= 0 || !pending_.isEmpty();
    const bool busy = importing || queue_.isRunning();
    const int row = clips_->currentRow(); const bool selected = row >= 0 && row < project_.clips.size();
    const bool ready = selected && project_.clips[row].ready;
    if (!selected) {
        const QSignalBlocker blocker(start_);
        start_->setDateTime(start_->minimumDateTime());
    }
    bool allReady = !project_.clips.isEmpty();
    for (const auto &c : project_.clips) allReady = allReady && c.ready;
    for (auto action : {addAction_,folderAction_,openAction_,toolsAction_}) action->setEnabled(!busy);
    removeAction_->setEnabled(selected && !busy); upAction_->setEnabled(row > 0 && !busy);
    downAction_->setEnabled(selected && row + 1 < project_.clips.size() && !busy);
    settingsPanel_->setEnabled(!busy); outputSettingsPanel_->setEnabled(!busy);
    calendarFields_->setVisible(project_.clock == ClockMode::Calendar);
    elapsedFields_->setVisible(project_.clock == ClockMode::Elapsed);
    emptyList_->setVisible(project_.clips.isEmpty());
    clips_->setVisible(!project_.clips.isEmpty());
    retry_->setVisible(!failed_.isEmpty());
    cancel_->setVisible(queue_.isRunning());
    progress_->setVisible(queue_.isRunning());
    const bool editable = selected && !busy && (project_.flow == TimeFlow::PerClip || row == 0);
    start_->setEnabled(editable && project_.clock == ClockMode::Calendar);
    elapsed_->setEnabled(editable && project_.clock == ClockMode::Elapsed);
    showDate_->setEnabled(project_.clock == ClockMode::Calendar);
    play_->setEnabled(ready); seek_->setEnabled(ready);
    export_->setEnabled(allReady && !busy); sample_->setEnabled(ready && !busy);
    retry_->setEnabled(!failed_.isEmpty() && !busy && allReady); cancel_->setEnabled(queue_.isRunning());
}
void MainWindow::moveClip(int delta) {
    const int row = clips_->currentRow(), target = row + delta;
    if (row < 0 || target < 0 || target >= project_.clips.size()) return;
    project_.clips.swapItemsAt(row,target); states_.swapItemsAt(row,target); details_.swapItemsAt(row,target); failed_.clear();
    rebuildList(); clips_->selectRow(target); selectClip(target); markDirty();
}
void MainWindow::removeClip() {
    const int row = clips_->currentRow(); if (row < 0) return;
    player_->stop(); player_->setSource({});
    project_.clips.removeAt(row); states_.removeAt(row); details_.removeAt(row); failed_.clear();
    rebuildList();
    if (!project_.clips.isEmpty()) clips_->selectRow(std::min(row,int(project_.clips.size()-1)));
    selectClip(clips_->currentRow()); markDirty();
}
void MainWindow::chooseTools() {
    const auto file = QFileDialog::getOpenFileName(this,QStringLiteral("เลือก ffmpeg.exe (โฟลเดอร์เดียวกับ ffprobe.exe)"),findTool("ffmpeg"),"ffmpeg (ffmpeg.exe)");
    if (file.isEmpty()) return;
    const auto probe = QFileInfo(file).dir().filePath("ffprobe.exe");
    if (!QFileInfo::exists(probe)) { QMessageBox::warning(this,"FFmpeg",QStringLiteral("ไม่พบ ffprobe.exe ในโฟลเดอร์เดียวกัน")); return; }
    QSettings settings; settings.setValue("ffmpegPath",file); settings.setValue("ffprobePath",probe);
    for (int i=0;i<project_.clips.size();++i) if (!project_.clips[i].ready) pending_.append(i);
    summary_->setText(QStringLiteral("ตั้งค่า FFmpeg แล้ว")); probeNext();
}
bool MainWindow::saveCurrent(bool saveAs) {
    QString file = projectFile_;
    if (file.isEmpty() || saveAs) file = QFileDialog::getSaveFileName(this,QStringLiteral("บันทึกโปรเจกต์"),file.isEmpty()?"project.vtime.json":file,"VideoTime Project (*.json)");
    if (file.isEmpty()) return false;
    QString error;
    if (!saveProject(file,project_,&error)) { QMessageBox::warning(this,QStringLiteral("บันทึกไม่ได้"),error); return false; }
    projectFile_ = file; dirty_ = false; setWindowTitle("VideoTime — " + QFileInfo(file).fileName()); return true;
}
bool MainWindow::maybeSave() {
    if (!dirty_) return true;
    const auto choice = QMessageBox::question(this,QStringLiteral("บันทึกโปรเจกต์"),QStringLiteral("ต้องการบันทึกการเปลี่ยนแปลงก่อนหรือไม่?"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    return choice != QMessageBox::Save || saveCurrent();
}
void MainWindow::openProject() {
    if (!maybeSave()) return;
    const auto file = QFileDialog::getOpenFileName(this,QStringLiteral("เปิดโปรเจกต์"),{},"VideoTime Project (*.json)");
    if (file.isEmpty()) return;
    Project loaded; QString error;
    if (!loadProject(file,&loaded,&error)) { QMessageBox::warning(this,QStringLiteral("เปิดไม่ได้"),error); return; }
    player_->stop(); player_->setSource({}); preview_->clear();
    project_ = loaded; projectFile_ = file; dirty_ = false; states_.clear(); details_.clear(); failed_.clear(); pending_.clear();
    for (int i=0;i<project_.clips.size();++i) { states_.append(QStringLiteral("รออ่านข้อมูล")); details_.append(QString()); pending_.append(i); }
    syncSettings(); rebuildList(); if (!project_.clips.isEmpty()) clips_->selectRow(0); updateOverlay(); probeNext();
    setWindowTitle("VideoTime — " + QFileInfo(file).fileName());
}
void MainWindow::runExport(bool sample, bool retry) {
    if (queue_.isRunning()) return;
    if (findTool("ffmpeg").isEmpty()) { chooseTools(); if (findTool("ffmpeg").isEmpty()) return; }
    if (project_.outputDirectory.trimmed().isEmpty() || !QDir().mkpath(project_.outputDirectory)) {
        QMessageBox::warning(this,QStringLiteral("ส่งออกไม่ได้"),QStringLiteral("กรุณาเลือกโฟลเดอร์ปลายทางที่เขียนไฟล์ได้")); return;
    }
    QVector<int> indices;
    if (retry && project_.output == OutputMode::Separate) { for (int i : failed_) indices.append(i); std::sort(indices.begin(),indices.end()); }
    player_->pause();
    queue_.start(project_, indices, sample?clips_->currentRow():-1, sample?player_->position():0);
    updateAvailability();
}
void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (!queue_.isRunning() && probingIndex_ < 0 && pending_.isEmpty() && event->mimeData()->hasUrls()) event->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent *event) {
    QStringList paths; for (const auto &url : event->mimeData()->urls()) if (url.isLocalFile()) paths.append(url.toLocalFile());
    addPaths(paths); event->acceptProposedAction();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (queue_.isRunning() && QMessageBox::question(this,QStringLiteral("กำลังส่งออก"),QStringLiteral("ยกเลิกงานส่งออกและปิดโปรแกรมหรือไม่?")) != QMessageBox::Yes) { event->ignore(); return; }
    if (!maybeSave()) { event->ignore(); return; }
    queue_.cancel(); probe_.stop(); player_->stop(); event->accept();
}
}
