#include "mainwindow.h"
#include "appearance.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QImage>
#include <QTimer>
int main(int argc, char **argv) {
    QLocale::setDefault(vt::arabicNumberLocale());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("VideoTime");
    QCoreApplication::setApplicationName("VideoTime");
    QCoreApplication::setApplicationVersion("0.1.0");
    vt::initializeAppearance();
    QCommandLineParser parser;
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("videos", "Video files to import", "[videos...]");
    parser.addOption({"screenshot", "Save a UI screenshot and exit (development)", "file"});
    parser.process(app);
    vt::MainWindow window;
    window.show();
    window.addPaths(parser.positionalArguments());
    if (parser.isSet("screenshot")) QTimer::singleShot(3500, &app, [&] {
        const bool ok = window.grab().save(parser.value("screenshot"));
        app.exit(ok ? 0 : 2);
    });
    return app.exec();
}
