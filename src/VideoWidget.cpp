#include "VideoWidget.h"

#include <QMetaObject>
#include <QPainter>
#include <algorithm>
#include <cstring>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
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
    update();
}

void VideoWidget::clear()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready = QImage();
    }
    m_front = QImage();
    update();
}

void VideoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (m_front.isNull())
        return;

    int w = width();
    int h = int(w / m_aspectRatio);
    if (h > height()) {
        h = height();
        w = int(h * m_aspectRatio);
    }
    const QRect target((width() - w) / 2, (height() - h) / 2, w, h);
    painter.drawImage(target, m_front);
}
