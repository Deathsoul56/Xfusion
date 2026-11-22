#include "OsdOverlay.h"

#include <QFontMetrics>
#include <QPainter>
#include <QTime>
#include <algorithm>

namespace {

constexpr int kPadding = 8;
constexpr int kMargin = 12;
constexpr int kGap = 6;

}

OsdOverlay::OsdOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        expire();
        update();
    });
}

void OsdOverlay::setOptions(const Options& options)
{
    m_options = options;
    if (options.messages == MessageMode::None)
        m_messages.clear();
    if (options.showTime || !m_messages.empty())
        m_timer.start();
    update();
}

void OsdOverlay::post(const QString& text)
{
    if (m_options.messages == MessageMode::None)
        return;
    Message message{text, {}};
    message.age.start();
    m_messages.push_back(std::move(message));
    while (static_cast<int>(m_messages.size()) > kMaxMessages)
        m_messages.pop_front();
    m_timer.start();
    update();
}

void OsdOverlay::expire()
{
    while (!m_messages.empty() && m_messages.front().age.elapsed() >= kMessageMs)
        m_messages.pop_front();
    if (m_messages.empty() && !m_options.showTime)
        m_timer.stop();
}

void OsdOverlay::setFps(double fps)
{
    if (m_fps == fps)
        return;
    m_fps = fps;
    update();
}

void OsdOverlay::setIndicators(bool paused, bool fastForward, bool rewinding)
{
    if (m_paused == paused && m_fastForward == fastForward && m_rewinding == rewinding)
        return;
    m_paused = paused;
    m_fastForward = fastForward;
    m_rewinding = rewinding;
    update();
}

void OsdOverlay::setPressedText(const QString& text)
{
    if (m_pressed == text)
        return;
    m_pressed = text;
    update();
}

// Draws one box against the edge of `area` and moves `offset` past it, so the next box stacks behind it.
void OsdOverlay::drawBox(QPainter& painter, const QString& text, const QRectF& area, Qt::Alignment alignment, int& offset, bool fromBottom)
{
    const QFontMetrics metrics(painter.font());
    const int maxWidth = int(area.width()) - 2 * kPadding;
    const QRect textRect = metrics.boundingRect(QRect(0, 0, maxWidth, 0), Qt::TextWordWrap, text);
    const int width = textRect.width() + 2 * kPadding;
    const int height = textRect.height() + kPadding;
    const qreal x = alignment & Qt::AlignRight ? area.right() - width : alignment & Qt::AlignHCenter ? area.center().x() - width / 2.0 : area.left();
    const qreal y = fromBottom ? area.bottom() - offset - height : area.top() + offset;
    const QRectF box(x, y, width, height);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(box, 5, 5);
    painter.setPen(Qt::white);
    painter.drawText(box.adjusted(kPadding, kPadding / 2.0, -kPadding, 0), Qt::TextWordWrap | Qt::AlignLeft, text);
    offset += height + kGap;
}

void OsdOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font = painter.font();
    font.setPointSize(std::max(6, m_options.size));
    painter.setFont(font);
    const QRectF area = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);

    if (m_options.messages == MessageMode::Onscreen) {
        const bool right = m_options.corner == OsdCorner::BottomRight || m_options.corner == OsdCorner::TopRight;
        const bool bottom = m_options.corner == OsdCorner::BottomLeft || m_options.corner == OsdCorner::BottomRight;
        int offset = 0;
        for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
            drawBox(painter, it->text, area, right ? Qt::AlignRight : Qt::AlignLeft, offset, bottom);
    }

    // The status overlays use a smaller size than the messages.
    font.setPointSize(std::max(6, m_options.size * 2 / 3));
    painter.setFont(font);
    int topLeft = 0;
    int topRight = 0;
    int bottomRight = 0;
    int bottomCenter = 0;
    if (m_options.showIndicators) {
        QString icons;
        if (m_rewinding)
            icons = QString::fromUtf8("\xE2\x97\x80\xE2\x97\x80");
        else if (m_fastForward)
            icons = QString::fromUtf8("\xE2\x96\xB6\xE2\x96\xB6");
        else if (m_paused)
            icons = QString::fromUtf8("\xE2\x9D\x9A\xE2\x9D\x9A");
        if (!icons.isEmpty())
            drawBox(painter, icons, area, Qt::AlignLeft, topLeft, false);
    }
    if (m_options.showFps && m_fps > 0)
        drawBox(painter, QString("%1 fps").arg(m_fps, 0, 'f', 1), area, Qt::AlignRight, topRight, false);
    if (m_options.showTime)
        drawBox(painter, QTime::currentTime().toString("HH:mm"), area, Qt::AlignRight, bottomRight, true);
    if (m_options.showPressed && !m_pressed.isEmpty())
        drawBox(painter, m_pressed, area, Qt::AlignHCenter, bottomCenter, true);
}
