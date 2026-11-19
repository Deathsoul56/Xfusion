#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QMessageBox>

#include "AppPaths.h"
#include "LibretroCore.h"
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Xfusion");
    QApplication::setApplicationName("Xfusion");

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption coreOption("core", "Path to a libretro core.", "path", AppPaths::defaultCorePath());
    parser.addOption(coreOption);
    parser.addPositionalArgument("game", "Game to open on startup.", "[game]");
    parser.process(app);

    LibretroCore core(defaultCoreProfile());
    QString error;
    if (!core.load(parser.value(coreOption), &error)) {
        QMessageBox::critical(nullptr, "Xfusion", error);
        return 1;
    }

    MainWindow window(core);
    window.show();
    if (!parser.positionalArguments().isEmpty())
        window.openGame(QFileInfo(parser.positionalArguments().first()).absoluteFilePath());

    return app.exec();
}
