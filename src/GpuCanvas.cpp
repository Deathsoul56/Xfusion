#include "GpuCanvas.h"

#include <QFile>
#include "PresetRenderer.h"
#include <cmath>
#include <cstring>
#include <rhi/qrhi.h>

namespace {

QShader loadShader(const char* name)
{
    QFile file(QString(":/shaders/%1.qsb").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QShader::fromSerialized(file.readAll());
}

const char* fragmentName(VideoShader shader)
{
    switch (shader) {
    case VideoShader::Scanlines: return "video_scanlines.frag";
    case VideoShader::Crt: return "video_crt.frag";
    case VideoShader::SharpBilinear: return "video_sharp.frag";
    case VideoShader::Lcd: return "video_lcd.frag";
    case VideoShader::None:
    case VideoShader::Preset: break;
    }
    return "video_plain.frag";
}

constexpr int kShaderCount = 5;
constexpr int kUniformBytes = 64 + 16 + 16; // clip correction, output size, source size

}

struct GpuCanvas::Resources {
    std::unique_ptr<QRhiBuffer> vertices;
    std::unique_ptr<QRhiBuffer> uniforms;
    std::unique_ptr<QRhiSampler> linear;
    std::unique_ptr<QRhiSampler> nearest;
    std::unique_ptr<QRhiTexture> texture;
    std::unique_ptr<QRhiShaderResourceBindings> bindLinear;
    std::unique_ptr<QRhiShaderResourceBindings> bindNearest;
    std::array<std::unique_ptr<QRhiGraphicsPipeline>, kShaderCount> pipelines;
    QRhiRenderPassDescriptor* passDescriptor = nullptr;
    bool verticesUploaded = false;
};

GpuCanvas::GpuCanvas(QWidget* parent)
    : QRhiWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

GpuCanvas::~GpuCanvas() = default;

void GpuCanvas::setFrame(const QImage& frame)
{
    m_frame = frame.format() == QImage::Format_RGB32 ? frame : frame.convertToFormat(QImage::Format_RGB32);
    m_uploadPending = true;
    update();
}

void GpuCanvas::setView(const QRectF& rect, bool smooth, VideoShader shader)
{
    if (m_rect == rect && m_smooth == smooth && m_shader == shader)
        return;
    m_rect = rect;
    m_smooth = smooth;
    m_shader = shader;
    update();
}

void GpuCanvas::setPreset(std::shared_ptr<const PresetProgram> program)
{
    m_program = std::move(program);
    m_preset.reset();
    m_uploadPending = !m_frame.isNull();
    update();
}

void GpuCanvas::setParameters(const QMap<QString, double>& values)
{
    m_values = values;
    update();
}

void GpuCanvas::clear()
{
    m_frame = QImage();
    m_uploadPending = false;
    update();
}

void GpuCanvas::initialize(QRhiCommandBuffer*)
{
    QRhi* rhi = this->rhi();
    if (m_r && m_r->passDescriptor != renderTarget()->renderPassDescriptor()) {
        // The render target changed (the window moved to another screen, say): what depends on it is made again.
        for (auto& pipeline : m_r->pipelines)
            pipeline.reset();
        m_r->passDescriptor = renderTarget()->renderPassDescriptor();
    }
    if (m_r)
        return;
    m_r = std::make_unique<Resources>();
    m_r->passDescriptor = renderTarget()->renderPassDescriptor();
    // A strip of two triangles over the whole viewport: position, then the texture coordinate (the image's top is v = 0).
    m_r->vertices.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 16 * sizeof(float)));
    m_r->vertices->create();
    m_r->uniforms.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
    m_r->uniforms->create();
    m_r->linear.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_r->linear->create();
    m_r->nearest.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_r->nearest->create();
}

void GpuCanvas::releaseResources()
{
    m_preset.reset();
    m_r.reset();
    m_uploadPending = !m_frame.isNull();
}

void GpuCanvas::ensureTexture(const QSize& size)
{
    if (m_r->texture && m_r->texture->pixelSize() == size)
        return;
    QRhi* rhi = this->rhi();
    m_r->texture.reset(rhi->newTexture(QRhiTexture::BGRA8, size));
    m_r->texture->create();
    const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    const auto bind = [&](std::unique_ptr<QRhiShaderResourceBindings>& target, QRhiSampler* sampler) {
        target.reset(rhi->newShaderResourceBindings());
        target->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, m_r->uniforms.get()),
                             QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_r->texture.get(), sampler)});
        target->create();
    };
    bind(m_r->bindLinear, m_r->linear.get());
    bind(m_r->bindNearest, m_r->nearest.get());
    // The pipelines were made against the bindings that are gone: they only need a layout that is the same.
    for (auto& pipeline : m_r->pipelines)
        pipeline.reset();
}

QRhiGraphicsPipeline* GpuCanvas::pipelineFor(VideoShader shader)
{
    if (shader == VideoShader::Preset)
        shader = VideoShader::None; // with no preset loaded the picture is drawn plain
    auto& slot = m_r->pipelines[static_cast<int>(shader)];
    if (slot)
        return slot.get();
    QRhi* rhi = this->rhi();
    slot.reset(rhi->newGraphicsPipeline());
    slot->setShaderStages({{QRhiShaderStage::Vertex, loadShader("video.vert")}, {QRhiShaderStage::Fragment, loadShader(fragmentName(shader))}});
    QRhiVertexInputLayout input;
    input.setBindings({{4 * sizeof(float)}});
    input.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}, {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}});
    slot->setVertexInputLayout(input);
    slot->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    slot->setShaderResourceBindings(m_r->bindLinear.get());
    slot->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    slot->create();
    return slot.get();
}

bool GpuCanvas::renderPreset(QRhiCommandBuffer* commands, const QRect& target)
{
    if (!m_preset)
        m_preset = std::make_unique<PresetRenderer>(rhi(), m_program);
    if (m_preset->error().isEmpty()) {
        m_preset->render(commands, renderTarget(), m_frame, m_uploadPending, target, m_smooth, m_values);
        m_uploadPending = false;
    }
    if (m_preset->error().isEmpty())
        return true;
    // The preset does not work on this GPU: say so, and draw with the built-in shader from now on.
    const QString message = m_preset->error();
    m_program.reset();
    m_preset.reset();
    m_uploadPending = true;
    QMetaObject::invokeMethod(this, [this, message] { emit presetFailed(message); }, Qt::QueuedConnection);
    return false;
}

void GpuCanvas::render(QRhiCommandBuffer* commands)
{
    if (!m_r)
        return;
    if (m_program && !m_frame.isNull() && !m_rect.isEmpty()) {
        const QSize pixelsOfTarget = renderTarget()->pixelSize();
        const double scale = width() > 0 ? double(pixelsOfTarget.width()) / width() : 1.0;
        const QRect target(int(std::lround(m_rect.x() * scale)), int(std::lround(m_rect.y() * scale)), int(std::lround(m_rect.width() * scale)),
                           int(std::lround(m_rect.height() * scale)));
        if (renderPreset(commands, target))
            return;
    }
    QRhi* rhi = this->rhi();
    QRhiResourceUpdateBatch* updates = rhi->nextResourceUpdateBatch();
    if (!m_r->verticesUploaded) {
        static const float quad[16] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
        updates->uploadStaticBuffer(m_r->vertices.get(), quad);
        m_r->verticesUploaded = true;
    }

    const bool drawing = !m_frame.isNull() && !m_rect.isEmpty();
    if (!m_frame.isNull()) {
        ensureTexture(m_frame.size());
        if (m_uploadPending) {
            updates->uploadTexture(m_r->texture.get(), QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(m_frame)));
            m_uploadPending = false;
        }
    }

    const QSize pixels = renderTarget()->pixelSize();
    const double ratio = width() > 0 ? double(pixels.width()) / width() : 1.0;
    const QRectF target(m_rect.x() * ratio, m_rect.y() * ratio, m_rect.width() * ratio, m_rect.height() * ratio);
    if (drawing) {
        float uniforms[24] = {};
        std::memcpy(uniforms, rhi->clipSpaceCorrMatrix().constData(), 16 * sizeof(float));
        uniforms[16] = float(target.width());
        uniforms[17] = float(target.height());
        uniforms[20] = float(m_frame.width());
        uniforms[21] = float(m_frame.height());
        updates->updateDynamicBuffer(m_r->uniforms.get(), 0, kUniformBytes, uniforms);
    }

    commands->beginPass(renderTarget(), Qt::black, {1.0f, 0}, updates);
    if (drawing) {
        commands->setGraphicsPipeline(pipelineFor(m_shader));
        // The viewport has its origin at the bottom left.
        commands->setViewport({float(target.x()), float(pixels.height() - target.y() - target.height()), float(target.width()), float(target.height())});
        const bool linear = m_smooth || m_shader == VideoShader::SharpBilinear;
        commands->setShaderResources(linear ? m_r->bindLinear.get() : m_r->bindNearest.get());
        const QRhiCommandBuffer::VertexInput input(m_r->vertices.get(), 0);
        commands->setVertexInput(0, 1, &input);
        commands->draw(4);
    }
    commands->endPass();
}
