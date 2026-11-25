#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QMessageBox>


#include "AppPaths.h"
#include "CoreRules.h"
#include "LibretroCore.h"
#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>

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
    QApplication::setApplicationVersion(XFUSION_VERSION);
    // Without this the taskbar button and the title bars fall back to a generic icon.
    QApplication::setWindowIcon(QIcon(":/app/icon.png"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption coreOption("core", "Use this libretro core for every game, instead of the one chosen for each system.", "path");
    parser.addOption(coreOption);
    QCommandLineOption statsOption("stats", "Log frames per second and audio buffer health to stderr.");
    parser.addOption(statsOption);
    parser.addPositionalArgument("game", "Game to open on startup.", "[game]");
    parser.process(app);

    // Two cores are used (ADR-0024); the one that plays a game is chosen when it opens. One is loaded now so that
    // the window can say which, and --core forces a single one for everything.
    LibretroCore core(defaultCoreProfile());
    QString error;
    const bool forced = parser.isSet(coreOption);
    if (forced) {
        const QString path = parser.value(coreOption);
        const bool genesisPlusGx = QFileInfo(path).fileName().contains("genesis_plus_gx", Qt::CaseInsensitive);
        if (!core.useCore(coreProfileById(genesisPlusGx ? CoreId::GenesisPlusGx : CoreId::PicoDrive), path, &error)) {
            QMessageBox::critical(nullptr, "Xfusion", error);
            return 1;
        }
    } else {
        for (const CoreInfo& info : allCores()) {
            if (AppPaths::coreInstalled(info.id) && core.useCore(coreProfileById(info.id), AppPaths::corePath(info.id), &error))
                break;
        }
        // Without a core the window still opens: the cores are downloaded from Options > Emulation > Manage Cores.
        if (!core.isLoaded() && !error.isEmpty())
            QMessageBox::warning(nullptr, "Xfusion", "A core could not be loaded:\n\n" + error);
    }

    MainWindow window(core);
    window.setCoreForced(forced);
    window.show();
    if (parser.isSet(statsOption))
        window.enableStats();
    if (!forced && !core.isLoaded() && parser.positionalArguments().isEmpty())
        window.offerCores();
    if (!parser.positionalArguments().isEmpty())
        window.openGame(QFileInfo(parser.positionalArguments().first()).absoluteFilePath());

    return app.exec();
}
