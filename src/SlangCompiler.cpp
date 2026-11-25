#include "SlangCompiler.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QVersionNumber>

namespace {

// The cache is only good for the Qt that made it, and for this format.
constexpr char kCacheFormat[] = "xfusion-slang-1";
constexpr int kCompilerTimeoutMs = 180'000;

const QMap<QString, QByteArray>& baked()
{
    static const QMap<QString, QByteArray> table = [] {
        QMap<QString, QByteArray> loaded;
        QFile file(kBakedShadersResource);
        if (file.open(QIODevice::ReadOnly)) {
            QDataStream in(&file);
            in >> loaded;
        }
        return loaded;
    }();
    return table;
}

SlangBaker& installedBaker()
{
    static SlangBaker baker;
    return baker;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

// Runs xfusion-shaderc on the two stages. `outputPath` is where it leaves the result.
SlangCompiled compileOutside(const SlangShader& shader, const QString& outputPath)
{
    SlangCompiled result;
#ifdef Q_OS_WIN
    const QString program = QDir(QCoreApplication::applicationDirPath()).filePath("xfusion-shaderc.exe");
#else
    const QString program = QDir(QCoreApplication::applicationDirPath()).filePath("xfusion-shaderc");
#endif
    if (!QFile::exists(program)) {
        result.error = "The shader compiler (xfusion-shaderc) was not found next to the program, so presets of your own cannot be compiled";
        return result;
    }
    QTemporaryDir directory;
    if (!directory.isValid()) {
        result.error = "A temporary folder for the shader compiler could not be made";
        return result;
    }
    const QString vertexPath = directory.filePath("shader.vert");
    const QString fragmentPath = directory.filePath("shader.frag");
    if (!writeFile(vertexPath, shader.vertex.toUtf8()) || !writeFile(fragmentPath, shader.fragment.toUtf8())) {
        result.error = "The shader could not be written for the compiler";
        return result;
    }
    QProcess process;
    process.start(program, {"compile", vertexPath, fragmentPath, outputPath});
    if (!process.waitForFinished(kCompilerTimeoutMs)) {
        process.kill();
        result.error = "The shader compiler took too long";
        return result;
    }
    if (process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit) {
        result.error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (result.error.isEmpty())
            result.error = "The shader compiler failed";
        return result;
    }
    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = "The result of the shader compiler could not be read";
        return result;
    }
    return deserializeCompiled(file.readAll());
}

}

QString slangCacheKey(const SlangShader& shader)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray(kCacheFormat));
    hash.addData(QVersionNumber::fromString(qVersion()).toString().toUtf8());
    hash.addData(shader.vertex.toUtf8());
    hash.addData(QByteArray(1, '\0'));
    hash.addData(shader.fragment.toUtf8());
    return QString::fromLatin1(hash.result().toHex());
}

QByteArray serializeCompiled(const SlangCompiled& compiled)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << compiled.vertex.serialized() << compiled.fragment.serialized();
    return bytes;
}

SlangCompiled deserializeCompiled(const QByteArray& bytes)
{
    SlangCompiled result;
    QDataStream in(bytes);
    QByteArray vertex, fragment;
    in >> vertex >> fragment;
    if (in.status() == QDataStream::Ok) {
        result.vertex = QShader::fromSerialized(vertex);
        result.fragment = QShader::fromSerialized(fragment);
    }
    if (!result.vertex.isValid() || !result.fragment.isValid())
        result.error = "A compiled shader could not be read";
    return result;
}

void setSlangBaker(SlangBaker baker)
{
    installedBaker() = std::move(baker);
}

SlangCompiled compileSlang(const SlangShader& shader, const QString& cacheDirectory)
{
    const QString key = slangCacheKey(shader);
    const auto inside = baked().constFind(key);
    if (inside != baked().constEnd()) {
        SlangCompiled found = deserializeCompiled(*inside);
        if (found.valid())
            return found;
    }
    QString cachePath;
    if (!cacheDirectory.isEmpty()) {
        cachePath = QDir(cacheDirectory).filePath(key + ".qsbs");
        QFile file(cachePath);
        if (file.open(QIODevice::ReadOnly)) {
            SlangCompiled found = deserializeCompiled(file.readAll());
            if (found.valid())
                return found;
        }
    }

    SlangCompiled result;
    if (installedBaker()) {
        result = installedBaker()(shader);
        if (result.valid() && !cachePath.isEmpty()) {
            QDir().mkpath(cacheDirectory);
            writeFile(cachePath, serializeCompiled(result));
        }
        return result;
    }
    if (!cachePath.isEmpty())
        QDir().mkpath(cacheDirectory);
    QTemporaryDir scratch;
    result = compileOutside(shader, cachePath.isEmpty() ? scratch.filePath("result.qsbs") : cachePath);
    return result;
}
