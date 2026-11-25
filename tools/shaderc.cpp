// The shader compiler of Xfusion: the only program that carries the baker of Qt Shader Tools (about 11 MB), so that xfusion.exe does not.
//   xfusion-shaderc compile <vertex.glsl> <fragment.glsl> <out.qsbs>
//       what xfusion.exe runs the first time it meets a shader of a preset of the user
//   xfusion-shaderc bake <out.bin>
//       compiles the shaders of the filters that come with the program (the presets under :/gpu-filters/) into a table that is
//       linked into xfusion.exe as a resource, when the program is built
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QMap>
#include <cstdio>

#include "GpuFilters.h"
#include "SlangBaker.h"
#include "SlangPreset.h"

namespace {

bool readText(const QString& path, QString* text)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *text = QString::fromUtf8(file.readAll());
    return true;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

int compile(const QString& vertexPath, const QString& fragmentPath, const QString& outputPath)
{
    SlangShader shader;
    if (!readText(vertexPath, &shader.vertex) || !readText(fragmentPath, &shader.fragment)) {
        std::fprintf(stderr, "The two stages of the shader could not be read\n");
        return 1;
    }
    const SlangCompiled compiled = bakeSlang(shader);
    if (!compiled.valid()) {
        std::fprintf(stderr, "%s\n", qPrintable(compiled.error));
        return 1;
    }
    if (!writeBytes(outputPath, serializeCompiled(compiled))) {
        std::fprintf(stderr, "%s could not be written\n", qPrintable(outputPath));
        return 1;
    }
    return 0;
}

int bake(const QString& outputPath)
{
    QMap<QString, QByteArray> table;
    for (const QString& presetPath : allGpuFilterPresets()) {
        const SlangPreset preset = SlangPreset::load(presetPath);
        if (!preset.error.isEmpty()) {
            std::fprintf(stderr, "%s: %s\n", qPrintable(presetPath), qPrintable(preset.error));
            return 1;
        }
        for (const SlangPass& pass : preset.passes) {
            const SlangShader shader = SlangShader::load(pass.shader);
            if (!shader.error.isEmpty()) {
                std::fprintf(stderr, "%s: %s\n", qPrintable(pass.shader), qPrintable(shader.error));
                return 1;
            }
            const QString key = slangCacheKey(shader);
            if (table.contains(key))
                continue;
            const SlangCompiled compiled = bakeSlang(shader);
            if (!compiled.valid()) {
                std::fprintf(stderr, "%s: %s\n", qPrintable(pass.shader), qPrintable(compiled.error));
                return 1;
            }
            table.insert(key, serializeCompiled(compiled));
        }
    }
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << table;
    if (!writeBytes(outputPath, bytes)) {
        std::fprintf(stderr, "%s could not be written\n", qPrintable(outputPath));
        return 1;
    }
    return 0;
}

}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 5 && args[1] == "compile")
        return compile(args[2], args[3], args[4]);
    if (args.size() == 3 && args[1] == "bake")
        return bake(args[2]);
    std::fprintf(stderr, "xfusion-shaderc compile <vertex> <fragment> <out>\nxfusion-shaderc bake <out>\n");
    return 2;
}
