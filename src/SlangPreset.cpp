#include "SlangPreset.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace {

constexpr int kMaxDepth = 16;

struct Raw {
    QMap<QString, QString> values;
    QMap<QString, QString> directories; // where each key was written: the paths in it start from there
};

QString readText(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QString("Cannot read %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString absolute(const QString& directory, const QString& path)
{
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : QDir(directory).filePath(path));
}

// What follows the = sign: a quoted text, or the rest of the line up to a comment.
QString valueOf(QString text)
{
    text = text.trimmed();
    if (text.startsWith('"')) {
        const qsizetype end = text.indexOf('"', 1);
        return text.mid(1, end < 0 ? -1 : end - 1);
    }
    for (const QString& marker : {QString(" #"), QString(" //"), QString("\t#")}) {
        const qsizetype cut = text.indexOf(marker);
        if (cut >= 0)
            text = text.left(cut);
    }
    return text.trimmed();
}

bool truthy(const QString& text)
{
    const QString value = text.trimmed().toLower();
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

SlangWrap wrapOf(const QString& text)
{
    const QString value = text.trimmed().toLower();
    if (value == "clamp_to_edge")
        return SlangWrap::ClampToEdge;
    if (value == "repeat")
        return SlangWrap::Repeat;
    if (value == "mirrored_repeat" || value == "mirror_repeat")
        return SlangWrap::MirroredRepeat;
    return SlangWrap::ClampToBorder;
}

SlangPass::Scale scaleOf(const QString& text)
{
    const QString value = text.trimmed().toLower();
    if (value == "viewport")
        return SlangPass::Scale::Viewport;
    if (value == "absolute")
        return SlangPass::Scale::Absolute;
    return SlangPass::Scale::Source;
}

bool readRaw(const QString& text, const QString& directory, int depth, Raw* raw, QString* error)
{
    if (depth > kMaxDepth) {
        *error = "Presets refer to each other too deeply";
        return false;
    }
    for (const QString& rawLine : text.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.startsWith("#reference")) {
            const QString target = absolute(directory, valueOf(line.mid(QString("#reference").size())));
            QString problem;
            const QString referenced = readText(target, &problem);
            if (referenced.isNull() && !problem.isEmpty()) {
                *error = problem;
                return false;
            }
            if (!readRaw(referenced, QFileInfo(target).absolutePath(), depth + 1, raw, error))
                return false;
            continue;
        }
        if (line.isEmpty() || line.startsWith('#') || line.startsWith("//"))
            continue;
        const qsizetype equals = line.indexOf('=');
        if (equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        raw->values[key] = valueOf(line.mid(equals + 1));
        raw->directories[key] = directory;
    }
    return true;
}

}

SlangPreset SlangPreset::load(const QString& path)
{
    QString error;
    const QString text = readText(path, &error);
    if (text.isNull()) {
        SlangPreset preset;
        preset.error = error;
        return preset;
    }
    return parse(text, QFileInfo(path).absolutePath());
}

SlangPreset SlangPreset::parse(const QString& text, const QString& directory, int depth)
{
    SlangPreset preset;
    Raw raw;
    if (!readRaw(text, directory, depth, &raw, &preset.error))
        return preset;
    const auto get = [&](const QString& key) { return raw.values.value(key); };
    const auto has = [&](const QString& key) { return raw.values.contains(key); };

    const int count = get("shaders").toInt();
    if (count <= 0) {
        preset.error = "The preset lists no shaders";
        return preset;
    }
    for (int i = 0; i < count; ++i) {
        const QString n = QString::number(i);
        if (!has("shader" + n)) {
            preset.error = QString("The preset has no shader%1").arg(i);
            return preset;
        }
        SlangPass pass;
        pass.shader = absolute(raw.directories.value("shader" + n), get("shader" + n));
        pass.filterSet = has("filter_linear" + n);
        pass.filterLinear = truthy(get("filter_linear" + n));
        pass.wrap = wrapOf(get("wrap_mode" + n));
        pass.mipmap = truthy(get("mipmap_input" + n));
        pass.alias = get("alias" + n);
        pass.floatFramebuffer = truthy(get("float_framebuffer" + n));
        pass.srgbFramebuffer = truthy(get("srgb_framebuffer" + n));
        pass.frameCountMod = get("frame_count_mod" + n).toInt();
        if (has("scale_type" + n) || has("scale_type_x" + n) || has("scale_type_y" + n) || has("scale" + n) || has("scale_x" + n) || has("scale_y" + n)) {
            pass.scaleTypeSet = true;
            pass.scaleTypeX = pass.scaleTypeY = scaleOf(get("scale_type" + n));
            if (has("scale_type_x" + n))
                pass.scaleTypeX = scaleOf(get("scale_type_x" + n));
            if (has("scale_type_y" + n))
                pass.scaleTypeY = scaleOf(get("scale_type_y" + n));
            pass.scaleX = pass.scaleY = has("scale" + n) ? get("scale" + n).toDouble() : 1.0;
            if (has("scale_x" + n))
                pass.scaleX = get("scale_x" + n).toDouble();
            if (has("scale_y" + n))
                pass.scaleY = get("scale_y" + n).toDouble();
        }
        preset.passes.append(pass);
    }
    for (const QString& name : get("textures").split(';', Qt::SkipEmptyParts)) {
        const QString key = name.trimmed();
        if (!has(key))
            continue;
        SlangTexture texture;
        texture.name = key;
        texture.path = absolute(raw.directories.value(key), get(key));
        texture.linear = !has(key + "_linear") || truthy(get(key + "_linear"));
        texture.wrap = wrapOf(get(key + "_wrap_mode"));
        texture.mipmap = truthy(get(key + "_mipmap"));
        preset.textures.append(texture);
    }
    for (const QString& name : get("parameters").split(';', Qt::SkipEmptyParts)) {
        const QString key = name.trimmed();
        if (has(key))
            preset.parameters[key] = get(key).toDouble();
    }
    return preset;
}

SlangShader SlangShader::load(const QString& path)
{
    QString error;
    const QString text = readText(path, &error);
    if (text.isNull()) {
        SlangShader shader;
        shader.error = error;
        return shader;
    }
    return parse(text, QFileInfo(path).absolutePath());
}

namespace {

// `#include "file"` lines replaced by the file, as the shader languages of RetroArch do.
bool expandIncludes(const QString& text, const QString& directory, int depth, QString* out, QString* error)
{
    static const QRegularExpression include(R"(^\s*#include\s+["<]([^">]+)[">])");
    if (depth > kMaxDepth) {
        *error = "Shaders include each other too deeply";
        return false;
    }
    for (const QString& line : text.split('\n')) {
        const QRegularExpressionMatch match = include.match(line);
        if (!match.hasMatch()) {
            *out += line + '\n';
            continue;
        }
        const QString target = absolute(directory, match.captured(1));
        QString problem;
        const QString included = readText(target, &problem);
        if (included.isNull()) {
            *error = problem;
            return false;
        }
        if (!expandIncludes(included, QFileInfo(target).absolutePath(), depth + 1, out, error))
            return false;
    }
    return true;
}

}

SlangShader SlangShader::parse(const QString& text, const QString& directory, QString* error)
{
    SlangShader shader;
    QString expanded;
    if (!expandIncludes(text, directory, 0, &expanded, &shader.error)) {
        if (error)
            *error = shader.error;
        return shader;
    }

    // The binding the push constants get: the one after the highest the shader uses.
    static const QRegularExpression bindingPattern(R"(binding\s*=\s*(\d+))");
    int highest = -1;
    for (auto match = bindingPattern.globalMatch(expanded); match.hasNext();)
        highest = std::max(highest, match.next().captured(1).toInt());
    static const QRegularExpression push(R"(layout\s*\(\s*push_constant\s*\))");
    static const QRegularExpression parameterPattern(R"RE(^\s*#pragma\s+parameter\s+(\S+)\s+"([^"]*)"\s+(\S+)\s+(\S+)\s+(\S+)(?:\s+(\S+))?)RE");

    QString common, vertex, fragment;
    QString* current = &common;
    bool sawStage = false;
    bool vertexStarted = false;
    bool fragmentStarted = false;
    for (const QString& line : expanded.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("#pragma stage")) {
            sawStage = true;
            // A stage starts from what came before the first stage line; going back to one that has started (an included
            // file may say `#pragma stage vertex` again) goes on where it was.
            if (trimmed.endsWith("vertex")) {
                if (!vertexStarted)
                    vertex = common;
                vertexStarted = true;
                current = &vertex;
            } else {
                if (!fragmentStarted)
                    fragment = common;
                fragmentStarted = true;
                current = &fragment;
            }
            continue;
        }
        if (trimmed.startsWith("#pragma parameter")) {
            const QRegularExpressionMatch match = parameterPattern.match(line);
            if (match.hasMatch()) {
                SlangParameter parameter;
                parameter.id = match.captured(1);
                parameter.description = match.captured(2);
                parameter.initial = match.captured(3).toDouble();
                parameter.minimum = match.captured(4).toDouble();
                parameter.maximum = match.captured(5).toDouble();
                parameter.step = match.captured(6).isEmpty() ? 0.0 : match.captured(6).toDouble();
                if (std::none_of(shader.parameters.begin(), shader.parameters.end(), [&](const SlangParameter& p) { return p.id == parameter.id; }))
                    shader.parameters.append(parameter);
            }
            continue;
        }
        if (trimmed.startsWith("#pragma name")) {
            shader.name = trimmed.mid(QString("#pragma name").size()).trimmed();
            continue;
        }
        if (trimmed.startsWith("#pragma format"))
            continue;
        QString fixed = line;
        fixed.replace(push, QString("layout(std140, set = 0, binding = %1)").arg(highest + 1));
        *current += fixed + '\n';
    }
    if (!sawStage || vertex.isEmpty() || fragment.isEmpty()) {
        shader.error = "Not a slang shader: it needs #pragma stage vertex and #pragma stage fragment";
        if (error)
            *error = shader.error;
        return shader;
    }
    shader.vertex = vertex;
    shader.fragment = fragment;
    return shader;
}
