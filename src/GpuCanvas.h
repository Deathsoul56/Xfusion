#pragma once

#include <QImage>
#include <QRhiWidget>
#include <memory>

#include "AppSettings.h"
#include "PresetProgram.h"

class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class PresetRenderer;

// Draws the picture with the GPU through Qt's RHI (ADR-0032): the frame is uploaded as a texture and drawn in the
// rectangle the display settings give, with one of the built-in shaders. The overlay of messages is a widget over it.
class GpuCanvas : public QRhiWidget {
    Q_OBJECT
public:
    explicit GpuCanvas(QWidget* parent = nullptr);
    ~GpuCanvas() override;

    // GUI thread. The frame as it is to be drawn (after the CPU filter).
    void setFrame(const QImage& frame);
    // `rect` is where the picture goes, in logical pixels of this widget.
    void setView(const QRectF& rect, bool smooth, VideoShader shader);
    void clear();
    // The picture goes through the passes of a preset instead of a built-in shader; null goes back to the built-in ones.
    void setPreset(std::shared_ptr<const PresetProgram> program);
    // The values of the parameters of the preset (the ones it declares and the user changed).
    void setParameters(const QMap<QString, double>& values);

signals:
    // The preset could not be built on this GPU; the built-in shader is used.
    void presetFailed(const QString& message);

protected:
    void initialize(QRhiCommandBuffer* commands) override;
    void render(QRhiCommandBuffer* commands) override;
    void releaseResources() override;

private:
    struct Resources;
    bool renderPreset(QRhiCommandBuffer* commands, const QRect& target);
    void ensureTexture(const QSize& size);
    QRhiGraphicsPipeline* pipelineFor(VideoShader shader);

    QImage m_frame;
    bool m_uploadPending = false;
    QRectF m_rect;
    bool m_smooth = true;
    VideoShader m_shader = VideoShader::None;
    std::unique_ptr<Resources> m_r;
    std::shared_ptr<const PresetProgram> m_program;
    std::unique_ptr<PresetRenderer> m_preset;
    QMap<QString, double> m_values;
};
