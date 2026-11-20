#include "VideoWidget.h"

#include <QMetaObject>
#include <QPainter>
#include <algorithm>
#include <cstring>

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
    m_osd->setGeometry(rect());
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
    update();
}

void VideoWidget::refilter()
{
    m_shown = applyVideoFilter(m_front, m_filter);
}

void VideoWidget::setDisplay(const DisplayOptions& options, bool smooth, VideoFilter filter)
{
    m_options = options;
    m_smooth = smooth;
    const bool filterChanged = filter != m_filter;
    m_filter = filter;
    if (filterChanged)
        refilter();
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
