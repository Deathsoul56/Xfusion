#include "PresetRenderer.h"

#include <QMatrix4x4>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <rhi/qrhi.h>

namespace {

constexpr int kVertexFloats = 6; // position (x, y, 0, 1), then the texture coordinate

// The quad in the way RetroArch gives it to its shaders: x and y from 0 to 1, and y = 0 is the top of the picture.
const float kQuad[4 * kVertexFloats] = {0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1};

QRhiSampler::AddressMode addressOf(int wrap)
{
    switch (static_cast<SlangWrap>(wrap)) {
    case SlangWrap::Repeat: return QRhiSampler::Repeat;
    case SlangWrap::MirroredRepeat: return QRhiSampler::Mirror;
    default: return QRhiSampler::ClampToEdge; // there is no border colour here: the edge is the nearest thing
    }
}

}

// What a shader asks for by name, and where it is.
struct PresetRenderer::Input {
    QRhiTexture* texture = nullptr;
    QSize size;
    bool linear = false;
    int wrap = static_cast<int>(SlangWrap::ClampToBorder);
};

struct PresetRenderer::Pass {
    const PresetProgram::Pass* source = nullptr;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    std::unique_ptr<QRhiShaderResourceBindings> layout; // what the pipeline was made against
    std::unique_ptr<QRhiShaderResourceBindings> bindings; // what is drawn with: the textures of this frame
    std::vector<QRhiTexture*> boundTextures;
    QRhiRenderPassDescriptor* pipelineTarget = nullptr;

    // What a member of a block of uniforms stands for, found out once when the preset is loaded and not on every frame.
    enum class Member { None, Mvp, FrameCount, FrameDirection, Parameter, SizeOutput, SizeSource, SizeOriginal, SizeViewport, SizePass, SizeTexture };
    struct Field {
        int offset = 0;
        Member kind = Member::None;
        int index = 0; // the pass or the texture, for the sizes of those
        QString name; // of the parameter
    };
    struct Block {
        int binding = 0;
        int size = 0;
        std::unique_ptr<QRhiBuffer> buffer;
        std::vector<Field> fields;
        std::vector<char> scratch;
    };
    std::vector<Block> blocks;
    // What a sampler reads.
    enum class Origin { None, Source, Original, History, Output, Feedback, Texture };
    struct Sampler {
        int binding;
        QString name;
        Origin kind = Origin::None;
        int index = 0;
    };
    std::vector<Sampler> samplers;

    // The output of the pass; two when a later pass wants the previous frame's.
    bool feedback = false;
    int current = 0;
    QSize size;
    std::unique_ptr<QRhiTexture> texture[2];
    std::unique_ptr<QRhiTextureRenderTarget> target[2];
    std::unique_ptr<QRhiRenderPassDescriptor> targetPass[2];
};

PresetRenderer::PresetRenderer(QRhi* rhi, std::shared_ptr<const PresetProgram> program)
    : m_rhi(rhi)
    , m_program(std::move(program))
{
    static const QRegularExpression historyPattern("^OriginalHistory(\\d+)$");
    static const QRegularExpression outputPattern("^PassOutput(\\d+)$");
    static const QRegularExpression feedbackPattern("^PassFeedback(\\d+)$");
    static const QRegularExpression historySizePattern("^OriginalHistorySize(\\d+)$");
    static const QRegularExpression outputSizePattern("^PassOutputSize(\\d+)$");
    static const QRegularExpression feedbackSizePattern("^PassFeedbackSize(\\d+)$");

    const int count = int(m_program->passes.size());
    for (int i = 0; i < count; ++i) {
        const SlangPass& config = m_program->passes[i].config;
        if (!config.alias.isEmpty())
            m_aliases[config.alias] = i;
        auto pass = std::make_unique<Pass>();
        pass->source = &m_program->passes[i];
        m_passes.push_back(std::move(pass));
    }
    // What each shader reads decides what has to be kept: the history of the picture, and the outputs of the passes of the frame before.
    for (int i = 0; i < count; ++i) {
        Pass& pass = *m_passes[i];
        for (const QShader* shader : {&pass.source->shaders.vertex, &pass.source->shaders.fragment}) {
            const QShaderDescription description = shader->description();
            for (const QShaderDescription::UniformBlock& block : description.uniformBlocks()) {
                auto found = std::find_if(pass.blocks.begin(), pass.blocks.end(), [&](const Pass::Block& b) { return b.binding == block.binding; });
                if (found == pass.blocks.end()) {
                    pass.blocks.emplace_back();
                    found = pass.blocks.end() - 1;
                    found->binding = block.binding;
                }
                found->size = std::max(found->size, int(block.size));
                for (const QShaderDescription::BlockVariable& member : block.members) {
                    const QString& name = member.name;
                    const bool known = std::any_of(found->fields.begin(), found->fields.end(), [&](const Pass::Field& f) { return f.offset == member.offset; });
                    if (known || member.offset + member.size > block.size)
                        continue;
                    Pass::Field field;
                    field.offset = member.offset;
                    field.name = name;
                    QRegularExpressionMatch match;
                    if (name == "MVP") {
                        field.kind = Pass::Member::Mvp;
                    } else if (name == "FrameCount") {
                        field.kind = Pass::Member::FrameCount;
                    } else if (name == "FrameDirection") {
                        field.kind = Pass::Member::FrameDirection;
                    } else if (m_program->defaults.contains(name)) {
                        field.kind = Pass::Member::Parameter;
                    } else if (name == "OutputSize") {
                        field.kind = Pass::Member::SizeOutput;
                    } else if (name == "SourceSize") {
                        field.kind = Pass::Member::SizeSource;
                    } else if (name == "OriginalSize" || historySizePattern.match(name).hasMatch()) {
                        field.kind = Pass::Member::SizeOriginal;
                    } else if (name == "FinalViewportSize") {
                        field.kind = Pass::Member::SizeViewport;
                    } else if ((match = outputSizePattern.match(name)).hasMatch() || (match = feedbackSizePattern.match(name)).hasMatch()) {
                        field.index = match.captured(1).toInt();
                        if (field.index < count)
                            field.kind = Pass::Member::SizePass;
                    } else if (name.endsWith("Size")) {
                        QString base = name.chopped(4);
                        if (base.endsWith("Feedback") && m_aliases.contains(base.chopped(8)))
                            base = base.chopped(8);
                        if (m_aliases.contains(base)) {
                            field.kind = Pass::Member::SizePass;
                            field.index = m_aliases.value(base);
                        } else {
                            for (int t = 0; t < int(m_program->textures.size()); ++t) {
                                if (m_program->textures[t].name == base) {
                                    field.kind = Pass::Member::SizeTexture;
                                    field.index = t;
                                }
                            }
                        }
                    }
                    if (field.kind != Pass::Member::None)
                        found->fields.push_back(field);
                }
            }
            for (const QShaderDescription::InOutVariable& sampled : description.combinedImageSamplers()) {
                const bool known = std::any_of(pass.samplers.begin(), pass.samplers.end(), [&](const Pass::Sampler& s) { return s.binding == sampled.binding; });
                if (known)
                    continue;
                Pass::Sampler input{sampled.binding, sampled.name};
                const QString& name = sampled.name;
                QRegularExpressionMatch match;
                if (name == "Source") {
                    input.kind = Pass::Origin::Source;
                } else if (name == "Original") {
                    input.kind = Pass::Origin::Original;
                } else if ((match = historyPattern.match(name)).hasMatch()) {
                    input.kind = Pass::Origin::History;
                    input.index = match.captured(1).toInt();
                    m_historyDepth = std::max(m_historyDepth, input.index + 1);
                } else if ((match = outputPattern.match(name)).hasMatch()) {
                    input.index = match.captured(1).toInt();
                    if (input.index < i)
                        input.kind = Pass::Origin::Output;
                } else if ((match = feedbackPattern.match(name)).hasMatch()) {
                    input.index = match.captured(1).toInt();
                    if (input.index < count) {
                        input.kind = Pass::Origin::Feedback;
                        m_passes[input.index]->feedback = true;
                    }
                } else if (m_aliases.contains(name)) {
                    input.index = m_aliases.value(name);
                    if (input.index < i)
                        input.kind = Pass::Origin::Output;
                } else if (name.endsWith("Feedback") && m_aliases.contains(name.chopped(8))) {
                    input.index = m_aliases.value(name.chopped(8));
                    input.kind = Pass::Origin::Feedback;
                    m_passes[input.index]->feedback = true;
                } else {
                    for (int t = 0; t < int(m_program->textures.size()); ++t) {
                        if (m_program->textures[t].name == name) {
                            input.kind = Pass::Origin::Texture;
                            input.index = t;
                        }
                    }
                }
                pass.samplers.push_back(input);
            }
        }
        for (Pass::Block& block : pass.blocks) {
            block.scratch.assign(size_t(block.size), 0);
            block.buffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, block.size));
            if (!block.buffer->create()) {
                fail("Could not create a uniform buffer");
                return;
            }
        }
    }

    m_vertices.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuad)));
    m_black.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
    if (!m_vertices->create() || !m_black->create()) {
        fail("Could not create the buffers of the renderer");
        return;
    }
    for (const QImage& image : m_program->images) {
        auto texture = std::unique_ptr<QRhiTexture>(m_rhi->newTexture(QRhiTexture::RGBA8, image.size()));
        if (!texture->create()) {
            fail("Could not create a texture of the preset");
            return;
        }
        m_userTextures.push_back(std::move(texture));
    }
}

PresetRenderer::~PresetRenderer() = default;

void PresetRenderer::fail(const QString& message)
{
    if (m_error.isEmpty())
        m_error = message;
}

QRhiSampler* PresetRenderer::sampler(bool linear, int wrap)
{
    const int key = (linear ? 1 : 0) + wrap * 2;
    auto it = m_samplers.find(key);
    if (it == m_samplers.end()) {
        const QRhiSampler::Filter filter = linear ? QRhiSampler::Linear : QRhiSampler::Nearest;
        std::unique_ptr<QRhiSampler> created(m_rhi->newSampler(filter, filter, QRhiSampler::None, addressOf(wrap), addressOf(wrap)));
        created->create();
        it = m_samplers.emplace(key, std::move(created)).first;
    }
    return it->second.get();
}

void PresetRenderer::ensureFrameResources(const QSize& size)
{
    if (m_frameSize == size && int(m_history.size()) == m_historyDepth)
        return;
    m_history.clear();
    m_frameSize = size;
    for (int i = 0; i < m_historyDepth; ++i) {
        std::unique_ptr<QRhiTexture> texture(m_rhi->newTexture(QRhiTexture::BGRA8, size));
        if (!texture->create()) {
            fail("Could not create the texture of the picture");
            return;
        }
        m_history.push_back(std::move(texture));
    }
    m_head = 0;
}

void PresetRenderer::ensureOutput(int index, const QSize& size)
{
    Pass& pass = *m_passes[index];
    if (pass.size == size && pass.texture[0])
        return;
    pass.size = size;
    pass.boundTextures.clear();
    const SlangPass& config = pass.source->config;
    const QRhiTexture::Format format = config.floatFramebuffer ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8;
    for (int i = 0; i < (pass.feedback ? 2 : 1); ++i) {
        pass.texture[i].reset(m_rhi->newTexture(format, size, 1, QRhiTexture::RenderTarget));
        pass.texture[i]->create();
        QRhiTextureRenderTargetDescription description{QRhiColorAttachment(pass.texture[i].get())};
        pass.target[i].reset(m_rhi->newTextureRenderTarget(description));
        pass.targetPass[i].reset(pass.target[i]->newCompatibleRenderPassDescriptor());
        pass.target[i]->setRenderPassDescriptor(pass.targetPass[i].get());
        if (!pass.target[i]->create()) {
            fail("Could not create a render target");
            return;
        }
    }
    // A pipeline is bound to the render pass it was made for.
    pass.pipeline.reset();
    pass.pipelineTarget = nullptr;
}

PresetRenderer::Input PresetRenderer::resolve(int passIndex, size_t samplerIndex, bool smooth) const
{
    const Pass::Sampler& sampler = m_passes[size_t(passIndex)]->samplers[samplerIndex];
    const SlangPass& config = m_passes[size_t(passIndex)]->source->config;
    Input input;
    input.linear = config.filterSet ? config.filterLinear : smooth;
    input.wrap = static_cast<int>(config.wrap);

    const auto original = [&](int back) {
        const int depth = int(m_history.size());
        input.texture = m_history[size_t(((m_head - back) % depth + depth) % depth)].get();
        input.size = m_frameSize;
    };
    const auto output = [&](int pass, bool previous) {
        const Pass& source = *m_passes[size_t(pass)];
        const int slot = previous && source.feedback ? 1 - source.current : source.current;
        input.texture = source.texture[slot].get();
        input.size = source.size;
    };

    switch (sampler.kind) {
    case Pass::Origin::Source:
        if (passIndex == 0)
            original(0);
        else
            output(passIndex - 1, false);
        break;
    case Pass::Origin::Original: original(0); break;
    case Pass::Origin::History: original(sampler.index); break;
    case Pass::Origin::Output: output(sampler.index, false); break;
    case Pass::Origin::Feedback: output(sampler.index, true); break;
    case Pass::Origin::Texture: {
        const SlangTexture& texture = m_program->textures[sampler.index];
        input.texture = m_userTextures[size_t(sampler.index)].get();
        input.size = m_program->images[sampler.index].size();
        input.linear = texture.linear;
        input.wrap = static_cast<int>(texture.wrap);
        break;
    }
    case Pass::Origin::None: break;
    }
    return input;
}

void PresetRenderer::buildPass(int index, QRhiRenderPassDescriptor* target)
{
    Pass& pass = *m_passes[index];
    const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    std::vector<QRhiShaderResourceBinding> bindings;
    for (const Pass::Block& block : pass.blocks)
        bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(block.binding, stages, block.buffer.get()));
    for (const Pass::Sampler& sampled : pass.samplers)
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(sampled.binding, stages, m_black.get(), sampler(false, 0)));
    pass.layout.reset(m_rhi->newShaderResourceBindings());
    pass.layout->setBindings(bindings.begin(), bindings.end());
    if (!pass.layout->create()) {
        fail("Could not create the bindings of a pass");
        return;
    }
    pass.pipeline.reset(m_rhi->newGraphicsPipeline());
    pass.pipeline->setShaderStages({{QRhiShaderStage::Vertex, pass.source->shaders.vertex}, {QRhiShaderStage::Fragment, pass.source->shaders.fragment}});
    QRhiVertexInputLayout input;
    input.setBindings({{kVertexFloats * sizeof(float)}});
    input.setAttributes({{0, 0, QRhiVertexInputAttribute::Float4, 0}, {0, 1, QRhiVertexInputAttribute::Float2, 4 * sizeof(float)}});
    pass.pipeline->setVertexInputLayout(input);
    pass.pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pass.pipeline->setShaderResourceBindings(pass.layout.get());
    pass.pipeline->setRenderPassDescriptor(target);
    if (!pass.pipeline->create()) {
        fail(QString("Could not create the pipeline of pass %1").arg(index));
        return;
    }
    pass.pipelineTarget = target;
    pass.bindings.reset();
    pass.boundTextures.clear();
}

void PresetRenderer::drawPass(QRhiCommandBuffer* commands, QRhiRenderTarget* finalTarget, int index, const QRect& viewport, bool smooth, const QMap<QString, double>& values)
{
    Pass& pass = *m_passes[index];
    const bool last = index == int(m_passes.size()) - 1;
    const SlangPass& config = pass.source->config;
    const QSize viewportSize = viewport.size();
    const QSize source = index == 0 ? m_frameSize : m_passes[index - 1]->size;

    QSize output = viewportSize;
    if (!last) {
        const SlangPass::Scale typeX = config.scaleTypeSet ? config.scaleTypeX : SlangPass::Scale::Source;
        const SlangPass::Scale typeY = config.scaleTypeSet ? config.scaleTypeY : SlangPass::Scale::Source;
        const auto axis = [&](SlangPass::Scale type, double scale, int sourceSize, int viewportSizeAxis) {
            switch (type) {
            case SlangPass::Scale::Viewport: return std::max(1, int(std::lround(viewportSizeAxis * scale)));
            case SlangPass::Scale::Absolute: return std::max(1, int(std::lround(scale)));
            default: return std::max(1, int(std::lround(sourceSize * scale)));
            }
        };
        output = QSize(axis(typeX, config.scaleX, source.width(), viewportSize.width()), axis(typeY, config.scaleY, source.height(), viewportSize.height()));
        ensureOutput(index, output);
        if (!m_error.isEmpty())
            return;
    } else {
        pass.size = output;
    }

    QRhiRenderPassDescriptor* targetPass = last ? finalTarget->renderPassDescriptor() : pass.targetPass[pass.current].get();
    // The two outputs of a pass have compatible render passes, so one pipeline serves both; the final pass has its own.
    if (!pass.pipeline || (last && pass.pipelineTarget != targetPass)) {
        buildPass(index, targetPass);
        if (!m_error.isEmpty())
            return;
    }

    // The values the blocks of uniforms hold.
    QMatrix4x4 mvp;
    mvp.translate(-1, 1);
    mvp.scale(2, -2);
    if (!last && m_rhi->isYUpInFramebuffer()) {
        // A texture that is rendered to is stored from the bottom when the framebuffer is Y-up: draw it upside down, so that it is read right.
        mvp.setToIdentity();
        mvp.translate(-1, -1);
        mvp.scale(2, 2);
    }
    mvp = m_rhi->clipSpaceCorrMatrix() * mvp;
    const unsigned frameCount = config.frameCountMod > 0 ? m_frameCount % unsigned(config.frameCountMod) : m_frameCount;
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    const auto sizeVector = [](const QSize& size, char* where) {
        if (size.width() <= 0 || size.height() <= 0)
            return;
        const float vec[4] = {float(size.width()), float(size.height()), 1.0f / float(size.width()), 1.0f / float(size.height())};
        std::memcpy(where, vec, sizeof(vec));
    };
    for (Pass::Block& block : pass.blocks) {
        char* data = block.scratch.data();
        for (const Pass::Field& field : block.fields) {
            char* where = data + field.offset;
            switch (field.kind) {
            case Pass::Member::Mvp: std::memcpy(where, mvp.constData(), 16 * sizeof(float)); break;
            case Pass::Member::FrameCount: std::memcpy(where, &frameCount, sizeof(frameCount)); break;
            case Pass::Member::FrameDirection: {
                const int one = 1;
                std::memcpy(where, &one, sizeof(one));
                break;
            }
            case Pass::Member::Parameter: {
                const float value = float(values.value(field.name));
                std::memcpy(where, &value, sizeof(value));
                break;
            }
            case Pass::Member::SizeOutput: sizeVector(output, where); break;
            case Pass::Member::SizeSource: sizeVector(source, where); break;
            case Pass::Member::SizeOriginal: sizeVector(m_frameSize, where); break;
            case Pass::Member::SizeViewport: sizeVector(viewportSize, where); break;
            case Pass::Member::SizePass: sizeVector(m_passes[size_t(field.index)]->size, where); break;
            case Pass::Member::SizeTexture: sizeVector(m_program->images[field.index].size(), where); break;
            case Pass::Member::None: break;
            }
        }
        updates->updateDynamicBuffer(block.buffer.get(), 0, block.size, data);
    }

    // The textures the pass reads this frame; the bindings are made again only when they change.
    std::vector<QRhiTexture*> textures;
    std::vector<Input> inputs;
    textures.reserve(pass.samplers.size());
    inputs.reserve(pass.samplers.size());
    for (size_t i = 0; i < pass.samplers.size(); ++i) {
        Input input = resolve(index, i, smooth);
        if (!input.texture)
            input.texture = m_black.get();
        textures.push_back(input.texture);
        inputs.push_back(input);
    }
    if (!pass.bindings || pass.boundTextures != textures) {
        const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
        std::vector<QRhiShaderResourceBinding> bindings;
        bindings.reserve(pass.blocks.size() + pass.samplers.size());
        for (const Pass::Block& block : pass.blocks)
            bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(block.binding, stages, block.buffer.get()));
        for (size_t i = 0; i < pass.samplers.size(); ++i)
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(pass.samplers[i].binding, stages, textures[i], sampler(inputs[i].linear, inputs[i].wrap)));
        pass.bindings.reset(m_rhi->newShaderResourceBindings());
        pass.bindings->setBindings(bindings.begin(), bindings.end());
        if (!pass.bindings->create()) {
            fail("Could not create the bindings of a pass");
            return;
        }
        pass.boundTextures = textures;
    }

    QRhiRenderTarget* renderTarget = last ? finalTarget : static_cast<QRhiRenderTarget*>(pass.target[pass.current].get());
    commands->beginPass(renderTarget, Qt::black, {1.0f, 0}, updates);
    commands->setGraphicsPipeline(pass.pipeline.get());
    if (last) {
        const QSize pixels = finalTarget->pixelSize();
        commands->setViewport({float(viewport.x()), float(pixels.height() - viewport.y() - viewport.height()), float(viewport.width()), float(viewport.height())});
    } else {
        commands->setViewport({0, 0, float(output.width()), float(output.height())});
    }
    commands->setShaderResources(pass.bindings.get());
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertices.get(), 0);
    commands->setVertexInput(0, 1, &vertexInput);
    commands->draw(4);
    commands->endPass();
}

void PresetRenderer::render(QRhiCommandBuffer* commands, QRhiRenderTarget* finalTarget, const QImage& frame, bool isNew, const QRect& viewport, bool smooth,
                            const QMap<QString, double>& values)
{
    if (!m_error.isEmpty() || frame.isNull() || viewport.isEmpty() || m_passes.empty())
        return;
    const bool resized = m_frameSize != frame.size() || int(m_history.size()) != m_historyDepth;
    ensureFrameResources(frame.size());
    if (!m_error.isEmpty())
        return;

    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    if (!m_staticUploaded) {
        updates->uploadStaticBuffer(m_vertices.get(), kQuad);
        QImage black(1, 1, QImage::Format_RGBA8888);
        black.fill(Qt::black);
        updates->uploadTexture(m_black.get(), black);
        for (size_t i = 0; i < m_userTextures.size(); ++i)
            updates->uploadTexture(m_userTextures[i].get(), m_program->images[int(i)]);
        m_staticUploaded = true;
    }
    if (resized) {
        for (auto& texture : m_history)
            updates->uploadTexture(texture.get(), QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(frame)));
        for (auto& pass : m_passes) {
            pass->pipeline.reset();
            pass->bindings.reset();
            pass->boundTextures.clear();
            pass->size = QSize();
            pass->texture[0].reset();
            pass->texture[1].reset();
        }
    } else if (isNew) {
        m_head = (m_head + 1) % int(m_history.size());
        updates->uploadTexture(m_history[m_head].get(), QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(frame)));
    }
    commands->resourceUpdate(updates);

    // Pass sizes are needed before the first one is drawn (a shader may ask for the size of a later pass's output of the frame before).
    for (int i = 0; i < int(m_passes.size()); ++i) {
        drawPass(commands, finalTarget, i, viewport, smooth, values);
        if (!m_error.isEmpty())
            return;
    }
    for (auto& pass : m_passes)
        if (pass->feedback)
            pass->current = 1 - pass->current;
    ++m_frameCount;
}
