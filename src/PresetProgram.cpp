#include "PresetProgram.h"

#include <QFileInfo>
#include <algorithm>

std::shared_ptr<PresetProgram> loadPresetProgram(const QString& path, const QString& cacheDirectory)
{
    auto program = std::make_shared<PresetProgram>();
    program->path = path;
    const SlangPreset preset = SlangPreset::load(path);
    if (!preset.error.isEmpty()) {
        program->error = preset.error;
        return program;
    }
    for (const SlangPass& pass : preset.passes) {
        SlangShader shader = SlangShader::load(pass.shader);
        if (!shader.error.isEmpty()) {
            program->error = QString("%1: %2").arg(QFileInfo(pass.shader).fileName(), shader.error);
            return program;
        }
        SlangCompiled compiled = compileSlang(shader, cacheDirectory);
        if (!compiled.valid()) {
            program->error = QString("%1: %2").arg(QFileInfo(pass.shader).fileName(), compiled.error);
            return program;
        }
        for (const SlangParameter& parameter : shader.parameters) {
            const bool known = std::any_of(program->parameters.begin(), program->parameters.end(), [&](const SlangParameter& p) { return p.id == parameter.id; });
            if (!known) {
                program->parameters.append(parameter);
                program->defaults[parameter.id] = parameter.initial;
            }
        }
        program->passes.append({pass, std::move(compiled)});
    }
    for (auto it = preset.parameters.begin(); it != preset.parameters.end(); ++it)
        if (program->defaults.contains(it.key()))
            program->defaults[it.key()] = it.value();
    for (const SlangTexture& texture : preset.textures) {
        QImage image(texture.path);
        if (image.isNull()) {
            program->error = QString("Cannot read the texture %1 (%2)").arg(texture.name, QFileInfo(texture.path).fileName());
            return program;
        }
        program->textures.append(texture);
        program->images.append(image.convertToFormat(QImage::Format_RGBA8888));
    }
    return program;
}
