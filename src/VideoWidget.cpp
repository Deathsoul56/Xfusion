#include "VideoWidget.h"

#include <QPainter>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
}

void VideoWidget::setFrame(const QImage& frame, double aspectRatio)
{
    m_frame = frame;
    m_aspectRatio = aspectRatio;
    update();
}

void VideoWidget::clear()
{
    m_frame = QImage();
    update();
}

void VideoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (m_frame.isNull())
        return;

    int w = width();
    int h = int(w / m_aspectRatio);
    if (h > height()) {
        h = height();
        w = int(h * m_aspectRatio);
    }
    const QRect target((width() - w) / 2, (height() - h) / 2, w, h);
    painter.drawImage(target, m_frame);
}
