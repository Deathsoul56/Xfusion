#pragma once

#include <QImage>
#include <QMap>
#include <QRect>
#include <QSize>
#include <QString>
#include <memory>
#include <vector>

#include "PresetProgram.h"

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiRenderTarget;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiTextureRenderTarget;

// Draws a picture through the passes of a preset (ADR-0032, phase B): every pass renders into a texture the next ones
// can read, the last one into the window, inside the rectangle the picture goes in. What the shaders may ask for is
// what the format of RetroArch defines: Source, Original, OriginalHistoryN, PassOutputN, PassFeedbackN, the aliases and
// the textures of the preset, with their sizes, and the frame counter and the parameters.
class PresetRenderer {
public:
    PresetRenderer(QRhi* rhi, std::shared_ptr<const PresetProgram> program);
    ~PresetRenderer();
    PresetRenderer(const PresetRenderer&) = delete;
    PresetRenderer& operator=(const PresetRenderer&) = delete;

    // Not empty when a pass could not be built; nothing is drawn then.
    const QString& error() const { return m_error; }

    // `frame` is the picture to draw (the same one again while no new one has arrived, `isNew` false). `viewport` is
    // the rectangle of the final pass in pixels of `finalTarget`, from its top left corner.
    void render(QRhiCommandBuffer* commands, QRhiRenderTarget* finalTarget, const QImage& frame, bool isNew, const QRect& viewport, bool smooth,
                const QMap<QString, double>& values);

private:
    struct Pass;
    struct Input;
    void fail(const QString& message);
    void buildPass(int index, QRhiRenderPassDescriptor* target);
    void ensureFrameResources(const QSize& size);
    void ensureOutput(int index, const QSize& size);
    QRhiSampler* sampler(bool linear, int wrap);
    Input resolve(int passIndex, size_t sampler, bool smooth) const;
    void drawPass(QRhiCommandBuffer* commands, QRhiRenderTarget* finalTarget, int index, const QRect& viewport, bool smooth, const QMap<QString, double>& values);

    QRhi* m_rhi;
    std::shared_ptr<const PresetProgram> m_program;
    QString m_error;

    std::unique_ptr<QRhiBuffer> m_vertices;
    std::unique_ptr<QRhiTexture> m_black;
    std::map<int, std::unique_ptr<QRhiSampler>> m_samplers;
    std::vector<std::unique_ptr<QRhiTexture>> m_userTextures;
    std::vector<std::unique_ptr<QRhiTexture>> m_history;
    int m_head = 0;
    int m_historyDepth = 1;
    QSize m_frameSize;
    bool m_staticUploaded = false;
    unsigned m_frameCount = 0;
    std::vector<std::unique_ptr<Pass>> m_passes;
    QMap<QString, int> m_aliases; // alias -> pass
};
