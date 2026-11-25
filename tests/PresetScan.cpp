// A development tool, not a test: reads and compiles every shader preset (.slangp) under a folder, or a sample of them,
// and says which fail and why. Use it on a copy of github.com/libretro/slang-shaders to see how much of it works.
//   preset_scan <folder> [sample-size] [seed]
//   preset_scan --dump <file.slang>      prints the two stages as they go to the compiler
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QMap>
#include <QRandomGenerator>
#include <cstdio>

#include "PresetProgram.h"
#include "SlangBaker.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    installSlangBaker();
    if (argc < 2) {
        std::fprintf(stderr, "preset_scan <folder> [sample-size] [seed]\n");
        return 2;
    }
    if (QString(argv[1]) == "--dump" && argc > 2) {
        const SlangShader shader = SlangShader::load(argv[2]);
        std::printf("// error: %s\n// ---- vertex ----\n%s\n// ---- fragment ----\n%s\n", qPrintable(shader.error), qPrintable(shader.vertex), qPrintable(shader.fragment));
        return 0;
    }
    QStringList presets;
    for (QDirIterator it(argv[1], {"*.slangp"}, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
        presets << it.next();
    presets.sort();
    const int sample = argc > 2 ? QString(argv[2]).toInt() : 0;
    if (sample > 0 && sample < presets.size()) {
        QRandomGenerator random(argc > 3 ? QString(argv[3]).toUInt() : 1u);
        QStringList chosen;
        while (chosen.size() < sample)
            chosen << presets.takeAt(int(random.bounded(quint32(presets.size()))));
        presets = chosen;
    }
    int ok = 0;
    QMap<QString, int> reasons;
    QElapsedTimer clock;
    clock.start();
    for (const QString& path : presets) {
        const std::shared_ptr<PresetProgram> program = loadPresetProgram(path, QString());
        if (program->error.isEmpty()) {
            ++ok;
            continue;
        }
        const QString first = program->error.section('\n', 0, 0).left(160);
        std::printf("FAIL %s\n     %s\n", qPrintable(QDir(argv[1]).relativeFilePath(path)), qPrintable(first));
        ++reasons[first.left(70)];
    }
    std::printf("\n%d of %lld presets read and compiled (%.0f s)\n", ok, (long long)presets.size(), clock.elapsed() / 1000.0);
    return 0;
}
