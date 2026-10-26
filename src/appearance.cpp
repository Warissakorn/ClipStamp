#include "appearance.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
namespace vt {
QLocale arabicNumberLocale() {
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::OmitGroupSeparator | QLocale::RejectGroupSeparator);
    return locale;
}
void initializeAppearance() {
    QLocale::setDefault(arabicNumberLocale());
    static const auto family = [] {
        const int id = QFontDatabase::addApplicationFont(":/fonts/NotoSansThai-Regular.ttf");
        const auto families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QStringLiteral("Leelawadee UI") : families.first();
    }();
    qApp->setStyle("Fusion");
    QFont font(family, 12);
    font.setFamilies({family, QStringLiteral("Segoe UI"), QStringLiteral("Leelawadee UI")});
    font.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(font);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f6f9"));
    palette.setColor(QPalette::WindowText, QColor("#202938"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f8fafc"));
    palette.setColor(QPalette::Text, QColor("#202938"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#344054"));
    palette.setColor(QPalette::Highlight, QColor("#eaf1ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#1e4fc4"));
    palette.setColor(QPalette::Link, QColor("#315fce"));
    palette.setColor(QPalette::ToolTipBase, QColor("#202938"));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    for (const auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#98a2b3"));
    qApp->setPalette(palette);
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget { color: #202938; }
        QMainWindow { background: #f4f6f9; }
        QWidget#appHeader { background: white; border-bottom: 1px solid #e5e9f0; }
        QFrame[card="true"] { background: white; border: 1px solid #e3e8ef; border-radius: 12px; }
        QLabel { background: transparent; }
        QLabel[role="heading"] { font-size: 16px; font-weight: 600; color: #202938; }
        QLabel[role="muted"] { color: #667085; font-size: 14px; }
        QLabel[role="fieldLabel"] { color: #475467; font-size: 14px; }
        QLabel[role="badge"] { background: #f0f3f8; border-radius: 10px; color: #667085; padding: 3px 10px; font-size: 12px; }
        QLabel#brand { font-family: 'Segoe UI'; font-size: 23px; font-weight: 700; color: #253a63; }
        QLabel#playTime { font-family: Consolas; font-size: 14px; color: #475467; }
        QPushButton, QToolButton { background: white; color: #344054; border: 1px solid #d9e0ea; border-radius: 7px; padding: 2px 10px; min-height: 26px; text-align: center; }
        QPushButton:hover, QToolButton:hover { background: #f5f8fd; border-color: #aabbd5; }
        QPushButton:pressed, QToolButton:pressed { background: #eaf1ff; }
        QPushButton:focus, QToolButton:focus { border: 1px solid #6089e5; }
        QPushButton:disabled, QToolButton:disabled { color: #98a2b3; background: #f8fafc; border-color: #e7ebf1; }
        QToolButton[role="quiet"], QPushButton[role="quiet"] { border-color: transparent; background: transparent; padding: 2px 8px; }
        QToolButton[role="quiet"]:hover, QPushButton[role="quiet"]:hover { background: #f0f4fa; }
        QPushButton#exportButton { background: #315fce; color: white; border-color: #315fce; font-weight: 600; padding: 2px 16px; }
        QPushButton#exportButton:hover { background: #264faf; border-color: #264faf; }
        QPushButton#exportButton:disabled { background: #e5ebf5; color: #8b9ab4; border-color: #e5ebf5; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateTimeEdit { background: white; color: #202938; border: 1px solid #d9e0ea; border-radius: 6px; padding: 2px 8px; min-height: 26px; selection-background-color: #dce8ff; selection-color: #1e3b70; }
        QAbstractSpinBox QLineEdit, QComboBox QLineEdit { border: none; padding: 0; min-height: 0; background: transparent; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateTimeEdit:focus { border-color: #6089e5; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QDateTimeEdit:disabled { background: #f8fafc; color: #98a2b3; border-color: #e7ebf1; }
        QComboBox::drop-down, QDateTimeEdit::drop-down { border: 0; width: 24px; }
        QComboBox::down-arrow, QDateTimeEdit::down-arrow { image: url(:/icons/chevron-down.svg); width: 14px; height: 14px; }
        QComboBox QAbstractItemView { background: white; color: #202938; selection-background-color: #eaf1ff; selection-color: #1e4fc4; border: 1px solid #d9e0ea; outline: none; padding: 4px; }
        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 19px; border: none; background: #f5f7fa; }
        QCheckBox { spacing: 8px; color: #475467; }
        QCheckBox::indicator { width: 17px; height: 17px; background: white; border: 1px solid #c5cfdd; border-radius: 4px; }
        QCheckBox::indicator:checked { background: #315fce; border-color: #315fce; image: url(:/icons/check.svg); }
        QCheckBox::indicator:disabled { background: #edf1f7; border-color: #d9e0ea; }
        QAbstractSpinBox::up-arrow { image: url(:/icons/chevron-up.svg); width: 10px; height: 10px; }
        QAbstractSpinBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 10px; height: 10px; }
        QToolButton#projectMenuButton { padding-right: 25px; }
        QToolButton#projectMenuButton::menu-indicator { image: url(:/icons/chevron-down.svg); width: 12px; height: 12px; subcontrol-origin: padding; subcontrol-position: right center; right: 7px; }
        QTabWidget::pane { border: none; background: white; }
        QTabBar::tab { background: #f1f4f8; color: #667085; border: none; border-radius: 6px; padding: 4px 22px; margin: 0 3px 6px 0; }
        QTabBar::tab:selected { background: #eaf1ff; color: #2854bb; }
        QTabBar::tab:hover { color: #2854bb; }
        QScrollArea { background: white; border: none; }
        QWidget#settingsTimePage, QWidget#settingsStylePage { background-color: white; }
        QScrollBar:vertical { background: transparent; width: 7px; margin: 2px; }
        QScrollBar::handle:vertical { background: #cbd3df; border-radius: 3px; min-height: 32px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QTableWidget { background: white; alternate-background-color: white; border: none; gridline-color: transparent; selection-background-color: #edf3ff; selection-color: #244ea6; outline: none; }
        QTableWidget::item { padding: 3px 5px; border-bottom: 1px solid #f0f3f7; }
        QTableWidget::item:selected { background: #edf3ff; color: #244ea6; }
        QHeaderView::section { background: #f8fafc; color: #667085; border: none; padding: 5px 5px; font-size: 12px; }
        QTableCornerButton::section { border: none; background: #f8fafc; }
        QSplitter::handle { background: transparent; width: 12px; }
        QProgressBar { background: #edf1f7; border: none; border-radius: 2px; max-height: 4px; min-height: 4px; }
        QProgressBar::chunk { background: #315fce; border-radius: 2px; }
        QSlider::groove:horizontal { height: 4px; background: #e1e7ef; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #6089e5; border-radius: 2px; }
        QSlider::handle:horizontal { background: #315fce; width: 12px; margin: -5px 0; border: 2px solid white; border-radius: 7px; }
        QMenu { background: white; border: 1px solid #e3e8ef; padding: 6px; }
        QMenu::item { padding: 9px 22px; border-radius: 4px; }
        QMenu::item:selected { background: #eaf1ff; color: #244ea6; }
        QMenu::item:disabled { color: #98a2b3; }
        QMenu::separator { height: 1px; background: #edf1f7; margin: 5px; }
        QToolTip { background: #202938; color: white; border: none; padding: 6px; }
        QTextEdit { background: white; color: #344054; border: 1px solid #e3e8ef; padding: 8px; }
        QCalendarWidget QWidget { background: white; color: #202938; }
        QCalendarWidget QToolButton { color: #344054; border: none; }
        QCalendarWidget QAbstractItemView { background: white; color: #202938; selection-background-color: #315fce; selection-color: white; }
    )"));
}
}
