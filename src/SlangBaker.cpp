#include "SlangBaker.h"

#include <QVector>
#include <rhi/qshaderbaker.h>

namespace {

QShader bake(const QString& source, QShader::Stage stage, QString* error)
{
    // Direct3D and Vulkan always; OpenGL with the oldest GLSL that can say what the shader says. Some shaders use what only
    // GLSL 4.x has (textureGather with a component): they still run on the other backends.
    const QVector<QVector<QShaderBaker::GeneratedShader>> attempts = {
        {{QShader::SpirvShader, QShaderVersion(100)}, {QShader::HlslShader, QShaderVersion(50)}, {QShader::GlslShader, QShaderVersion(330)}, {QShader::GlslShader, QShaderVersion(150)}},
        {{QShader::SpirvShader, QShaderVersion(100)}, {QShader::HlslShader, QShaderVersion(50)}, {QShader::GlslShader, QShaderVersion(450)}, {QShader::GlslShader, QShaderVersion(400)}},
        {{QShader::SpirvShader, QShaderVersion(100)}, {QShader::HlslShader, QShaderVersion(50)}}};
    QString message;
    for (const auto& targets : attempts) {
        QShaderBaker baker;
        baker.setGeneratedShaderVariants({QShader::StandardShader});
        baker.setGeneratedShaders(targets);
        baker.setSourceString(source.toUtf8(), stage);
        const QShader shader = baker.bake();
        if (shader.isValid())
            return shader;
        if (message.isEmpty())
            message = baker.errorMessage().trimmed();
    }
    if (error->isEmpty())
        *error = QString("%1 shader: %2").arg(stage == QShader::VertexStage ? "Vertex" : "Fragment", message);
    return {};
}

}

SlangCompiled bakeSlang(const SlangShader& shader)
{
    SlangCompiled result;
    result.vertex = bake(shader.vertex, QShader::VertexStage, &result.error);
    if (result.error.isEmpty())
        result.fragment = bake(shader.fragment, QShader::FragmentStage, &result.error);
    return result;
}

void installSlangBaker()
{
    setSlangBaker(bakeSlang);
}
