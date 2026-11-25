#include "OsdOverlay.h"

#include <QFontMetrics>
#include <QPainter>
#include <QTime>
#include <algorithm>

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

void OsdOverlay::post(const QString& text, int durationMs, const QImage& icon)
{
    if (m_options.messages == MessageMode::None)
        return;
    Message message{text, {}, durationMs > 0 ? durationMs : kMessageMs, icon};
    message.age.start();
    m_messages.push_back(std::move(message));
    while (static_cast<int>(m_messages.size()) > kMaxMessages)
        m_messages.pop_front();
    m_timer.start();
    update();
}

void OsdOverlay::expire()
{
    std::erase_if(m_messages, [](const Message& message) { return message.age.elapsed() >= message.lifeMs; });
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

// Draws one box against the edge of `area` and moves `offset` past it, so the next box stacks behind it. The measures
// are those of the on-screen display of snes9xrd: the size is the height of a line in pixels, the padding and the
// gap between boxes follow from it (size / 2.4, and half of that), the picture of a message is twice the size, and the
// first line of a message of several lines (the title of the game) is drawn 1.2 times bigger.
void OsdOverlay::drawBox(QPainter& painter, const QString& text, const QRectF& area, Qt::Alignment alignment, int& offset, bool fromBottom, const QImage& icon, int pixelSize)
{
    const int padding = std::max(1, int(pixelSize / 2.4));
    const int gap = padding / 2;
    QFont body = painter.font();
    body.setPixelSize(pixelSize);
    QFont title = body;
    title.setPixelSize(std::max(1, int(pixelSize * 1.2)));
    const QFontMetrics bodyMetrics(body);
    const QFontMetrics titleMetrics(title);

    const int iconSide = icon.isNull() ? 0 : pixelSize * 2;
    const int iconSpace = iconSide > 0 ? iconSide + padding : 0;
    const int maxWidth = std::max(pixelSize, int(area.width()) - 2 * padding - iconSpace);
    struct Line {
        QString text;
        const QFont* font;
        int width, height;
    };
    QVector<Line> lines;
    const QStringList parts = text.split('\n');
    for (int i = 0; i < parts.size(); ++i) {
        const bool isTitle = i == 0 && parts.size() > 1;
        const QFontMetrics& metrics = isTitle ? titleMetrics : bodyMetrics;
        const int slot = isTitle ? int(pixelSize * 1.2) : pixelSize;
        const QRect rect = metrics.boundingRect(QRect(0, 0, maxWidth, 0), Qt::TextWordWrap, parts[i]);
        const int wrapped = std::max(1, rect.height() / std::max(1, metrics.lineSpacing()));
        lines.append({parts[i], isTitle ? &title : &body, std::min(rect.width(), maxWidth), wrapped == 1 ? slot : wrapped * metrics.lineSpacing()});
    }
    int textWidth = 0, textHeight = 0;
    for (const Line& line : lines) {
        textWidth = std::max(textWidth, line.width);
        textHeight += line.height;
    }
    const int width = textWidth + iconSpace + 2 * padding;
    const int height = std::max(textHeight, iconSide) + 2 * padding;
    const qreal x = alignment & Qt::AlignRight ? area.right() - width : alignment & Qt::AlignHCenter ? area.center().x() - width / 2.0 : area.left();
    const qreal y = fromBottom ? area.bottom() - offset - height : area.top() + offset;
    const QRectF box(x, y, width, height);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 0x88));
    painter.drawRoundedRect(box, padding / 2.0, padding / 2.0);
    if (iconSide > 0)
        painter.drawImage(QRectF(box.left() + padding, box.top() + (height - iconSide) / 2.0, iconSide, iconSide), icon);
    painter.setPen(Qt::white);
    qreal lineY = box.top() + padding;
    for (const Line& line : lines) {
        painter.setFont(*line.font);
        painter.drawText(QRectF(box.left() + padding + iconSpace, lineY, maxWidth, line.height), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, line.text);
        lineY += line.height;
    }
    offset += height + gap;
}

void OsdOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    // The size is in pixels of the screen, as in snes9xrd, whatever the scaling of the desktop is.
    const int size = std::max(6, int(m_options.size / devicePixelRatioF()));
    const int margin = std::max(1, int(size / 2.4));
    const QRectF area = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

    if (m_options.messages == MessageMode::Onscreen) {
        const bool right = m_options.corner == OsdCorner::BottomRight || m_options.corner == OsdCorner::TopRight;
        const bool bottom = m_options.corner == OsdCorner::BottomLeft || m_options.corner == OsdCorner::BottomRight;
        int offset = 0;
        for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
            drawBox(painter, it->text, area, right ? Qt::AlignRight : Qt::AlignLeft, offset, bottom, it->icon, size);
    }

    // The status overlays (frame rate, clock, indicators, pressed keys) are the same size as the messages, as in snes9xrd.
    const int statusSize = size;
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
            drawBox(painter, icons, area, Qt::AlignLeft, topLeft, false, QImage(), statusSize);
    }
    if (m_options.showFps && m_fps > 0)
        drawBox(painter, QString("%1 fps").arg(m_fps, 0, 'f', 1), area, Qt::AlignRight, topRight, false, QImage(), statusSize);
    if (m_options.showTime)
        drawBox(painter, QTime::currentTime().toString("HH:mm"), area, Qt::AlignRight, bottomRight, true, QImage(), statusSize);
    if (m_options.showPressed && !m_pressed.isEmpty())
        drawBox(painter, m_pressed, area, Qt::AlignHCenter, bottomCenter, true, QImage(), statusSize);
}
