#include "VideoWidget.h"

#include <QMetaObject>
#include <QPainter>
#include <algorithm>
#include <cstring>

#include "GpuCanvas.h"
#include "OsdOverlay.h"
#include "VideoFilters.h"

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
    , m_osd(new OsdOverlay(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
    m_osd->setGeometry(rect());
}

void VideoWidget::resizeEvent(QResizeEvent*)
{
    if (m_gpu)
        m_gpu->setGeometry(rect());
    m_osd->setGeometry(rect());
    updateGpuView();
}

void VideoWidget::setDriver(VideoDriver driver)
{
    if (m_driverChosen)
        return;
    m_driverChosen = true;
    if (driver == VideoDriver::Software)
        return;
    m_gpu = new GpuCanvas(this);
    switch (driver) {
    case VideoDriver::Direct3D11: m_gpu->setApi(QRhiWidget::Api::Direct3D11); break;
    case VideoDriver::Direct3D12: m_gpu->setApi(QRhiWidget::Api::Direct3D12); break;
    case VideoDriver::Vulkan: m_gpu->setApi(QRhiWidget::Api::Vulkan); break;
    case VideoDriver::OpenGL: m_gpu->setApi(QRhiWidget::Api::OpenGL); break;
    case VideoDriver::Auto:
    case VideoDriver::Software: break;
    }
    m_gpu->setGeometry(rect());
    m_osd->raise();
    connect(m_gpu, &GpuCanvas::presetFailed, this, [this](const QString& message) { m_osd->post("Shader preset: " + message, 6000); });
    connect(m_gpu, &QRhiWidget::renderFailed, this, [this] {
        m_gpu->hide();
        m_gpu->deleteLater();
        m_gpu = nullptr;
        m_osd->post("The GPU could not draw the picture; using the software renderer", 6000);
        update();
    });
    m_gpu->show();
}

void VideoWidget::setShaderPreset(std::shared_ptr<const PresetProgram> program, const QMap<QString, double>& values)
{
    if (!m_gpu)
        return;
    m_gpu->setPreset(std::move(program));
    m_gpu->setParameters(values);
}

void VideoWidget::setShaderParameters(const QMap<QString, double>& values)
{
    if (m_gpu)
        m_gpu->setParameters(values);
}

void VideoWidget::updateGpuView()
{
    if (!m_gpu)
        return;
    const QRect shown = m_front.isNull() ? QRect() : displayRect(size(), m_front.size(), m_aspectRatio, m_options);
    m_gpu->setView(shown, m_smooth, m_shader);
}

void VideoWidget::submitFrame(const QImage& frame, double aspectRatio)
{
    if (m_back.size() != frame.size() || m_back.format() != frame.format())
        m_back = QImage(frame.size(), frame.format());
    const qsizetype rowBytes = std::min(m_back.bytesPerLine(), frame.bytesPerLine());
    for (int y = 0; y < frame.height(); ++y)
        std::memcpy(m_back.scanLine(y), frame.constScanLine(y), static_cast<size_t>(rowBytes));

    bool schedule = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(m_back, m_ready);
        m_readyAspect = aspectRatio;
        schedule = !m_updateQueued;
        m_updateQueued = true;
    }
    if (schedule)
        QMetaObject::invokeMethod(this, &VideoWidget::takeReadyFrame, Qt::QueuedConnection);
}

void VideoWidget::takeReadyFrame()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(m_ready, m_front);
        m_aspectRatio = m_readyAspect;
        m_updateQueued = false;
    }
    m_shown = applyVideoFilter(m_front, m_filter);
    if (m_gpu) {
        m_gpu->setFrame(m_shown);
        updateGpuView();
    }
    update();
}

void VideoWidget::clear()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready = QImage();
    }
    m_front = QImage();
    m_shown = QImage();
    if (m_gpu) {
        m_gpu->clear();
        updateGpuView();
    }
    update();
}

void VideoWidget::refilter()
{
    m_shown = applyVideoFilter(m_front, m_filter);
    if (m_gpu && !m_shown.isNull())
        m_gpu->setFrame(m_shown);
}

void VideoWidget::setDisplay(const DisplayOptions& options, bool smooth, VideoFilter filter, VideoShader shader)
{
    m_options = options;
    m_smooth = smooth;
    m_shader = shader;
    const bool filterChanged = filter != m_filter;
    m_filter = filter;
    if (filterChanged)
        refilter();
    updateGpuView();
    update();
}

void VideoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (m_shown.isNull())
        return;

    // Sharp bilinear is only sharp if the final scaling is smooth.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_smooth || m_filter == VideoFilter::SharpBilinear4x);
    painter.drawImage(displayRect(size(), m_front.size(), m_aspectRatio, m_options), m_shown);
}
