#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QMessageBox>

#include "AppPaths.h"
#include "LibretroCore.h"
#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>

// The exe is a GUI program with no console of its own. Started from a terminal, it borrows that terminal so
// --stats can print there; redirected output (a file or a pipe) is left alone.
static void attachParentConsole()
{
    if (GetStdHandle(STD_ERROR_HANDLE) || !AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONOUT$", "w", stdout);
}
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    attachParentConsole();
#endif
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Xfusion");
    QApplication::setApplicationName("Xfusion");

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption coreOption("core", "Path to a libretro core.", "path", AppPaths::defaultCorePath());
    parser.addOption(coreOption);
    QCommandLineOption statsOption("stats", "Log frames per second and audio buffer health to stderr.");
    parser.addOption(statsOption);
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
    if (parser.isSet(statsOption))
        window.enableStats();
    if (!parser.positionalArguments().isEmpty())
        window.openGame(QFileInfo(parser.positionalArguments().first()).absoluteFilePath());

    return app.exec();
}
