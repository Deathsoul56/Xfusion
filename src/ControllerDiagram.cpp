#include "ControllerDiagram.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>

namespace {

constexpr double kMargin = 10.0;
constexpr double kLabelStrip = 30.0; // room above the picture for buttons that are not on the pad

// Highlight colour per button, like the printed button caps.
QColor glowColor(PadButton button, const QColor& fallback)
{
    switch (button) {
    case PadButton::A:
    case PadButton::B:
    case PadButton::C:
        return QColor("#e0483f");
    case PadButton::X:
    case PadButton::Y:
    case PadButton::Z:
        return QColor("#8f95a3");
    default:
        return fallback;
    }
}

}

ControllerDiagram::ControllerDiagram(QWidget* parent)
    : QWidget(parent)
    , m_threeButton(":/controllers/genesis_3button.png")
    , m_sixButton(":/controllers/genesis_6button.png")
    , m_masterSystem(":/controllers/mastersystem.png")
{
}

void ControllerDiagram::setGroup(InputGroup group)
{
    m_group = group;
    update();
}

void ControllerDiagram::setPadType(PadType type)
{
    m_padType = type;
    update();
}

void ControllerDiagram::setPressed(const std::array<bool, kPadButtonCount>& pressed)
{
    if (pressed == m_pressed)
        return;
    m_pressed = pressed;
    update();
}

const QPixmap& ControllerDiagram::picture() const
{
    if (m_group == InputGroup::MasterSystem)
        return m_masterSystem;
    return m_padType == PadType::SixButton ? m_sixButton : m_threeButton;
}

const std::vector<ControllerDiagram::Hotspot>& ControllerDiagram::hotspots() const
{
    // Measured on the pictures in resources/controllers; the d-pad entries sit on the arms of the cross.
    static const std::vector<Hotspot> three = {
        {PadButton::Up, 0.2176, 0.344, 0.055},
        {PadButton::Down, 0.2176, 0.540, 0.055},
        {PadButton::Left, 0.163, 0.442, 0.055},
        {PadButton::Right, 0.272, 0.442, 0.055},
        {PadButton::Start, 0.717, 0.200, 0.050},
        {PadButton::A, 0.683, 0.537, 0.080},
        {PadButton::B, 0.782, 0.445, 0.080},
        {PadButton::C, 0.884, 0.366, 0.080},
    };
    static const std::vector<Hotspot> six = {
        {PadButton::Up, 0.247, 0.347, 0.055},
        {PadButton::Down, 0.247, 0.496, 0.055},
        {PadButton::Left, 0.207, 0.421, 0.055},
        {PadButton::Right, 0.287, 0.421, 0.055},
        {PadButton::Start, 0.473, 0.387, 0.050},
        {PadButton::X, 0.633, 0.377, 0.075},
        {PadButton::Y, 0.712, 0.311, 0.075},
        {PadButton::Z, 0.801, 0.282, 0.075},
        {PadButton::A, 0.668, 0.587, 0.075},
        {PadButton::B, 0.756, 0.476, 0.075},
        {PadButton::C, 0.857, 0.440, 0.075},
    };
    // The two buttons are labelled 1 and 2 on the pad; PicoDrive takes them as B and C.
    static const std::vector<Hotspot> masterSystem = {
        {PadButton::Up, 0.257, 0.321, 0.095},
        {PadButton::Down, 0.257, 0.663, 0.095},
        {PadButton::Left, 0.185, 0.492, 0.095},
        {PadButton::Right, 0.326, 0.492, 0.095},
        {PadButton::B, 0.693, 0.622, 0.140},
        {PadButton::C, 0.861, 0.622, 0.140},
    };
    if (m_group == InputGroup::MasterSystem)
        return masterSystem;
    return m_padType == PadType::SixButton ? six : three;
}

std::optional<ControllerDiagram::OffPadButton> ControllerDiagram::offPadButton() const
{
    // The Master System has its pause button on the console, not on the pad.
    if (m_group == InputGroup::MasterSystem)
        return OffPadButton{PadButton::Start, "PAUSE (console)"};
    // The 6 button picture has no MODE button on its face.
    if (m_padType == PadType::SixButton)
        return OffPadButton{PadButton::Mode, "MODE"};
    return std::nullopt;
}

QRectF ControllerDiagram::pictureRect() const
{
    const QPixmap& image = picture();
    const double aspect = double(image.width()) / image.height();
    const double top = kLabelStrip;
    double width = this->width() - kMargin * 2;
    double height = width / aspect;
    if (height > this->height() - top - kMargin) {
        height = this->height() - top - kMargin;
        width = height * aspect;
    }
    return {(this->width() - width) / 2, top + (this->height() - top - kMargin - height) / 2, width, height};
}

QPointF ControllerDiagram::hotspotCentre(const Hotspot& hotspot, const QRectF& picture) const
{
    return {picture.left() + picture.width() * hotspot.x, picture.top() + picture.height() * hotspot.y};
}

QRectF ControllerDiagram::offPadLabelRect() const
{
    const auto label = offPadButton();
    const QRectF image = pictureRect();
    QFont font = this->font();
    font.setBold(true);
    const double width = QFontMetricsF(font).horizontalAdvance(label ? label->text : "") + 28;
    const double height = 22;
    return {image.center().x() - width / 2, image.top() - height - 6, width, height};
}

void ControllerDiagram::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF image = pictureRect();
    painter.drawPixmap(image, picture(), QRectF(picture().rect()));

    const QColor accent = palette().color(QPalette::Highlight);
    for (const Hotspot& hotspot : hotspots()) {
        if (!m_pressed[static_cast<std::size_t>(hotspot.button)])
            continue;
        const QPointF centre = hotspotCentre(hotspot, image);
        const double radius = image.height() * hotspot.radius;
        const QColor glow = glowColor(hotspot.button, accent);

        QRadialGradient halo(centre, radius * 1.9);
        halo.setColorAt(0.0, QColor(glow.red(), glow.green(), glow.blue(), 170));
        halo.setColorAt(1.0, QColor(glow.red(), glow.green(), glow.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawEllipse(centre, radius * 1.9, radius * 1.9);

        QColor fill = glow.lighter(130);
        fill.setAlpha(150);
        painter.setPen(QPen(QColor(255, 255, 255, 235), 2.5));
        painter.setBrush(fill);
        painter.drawEllipse(centre, radius, radius);
    }

    const auto offPad = offPadButton();
    if (!offPad)
        return;
    const QRectF label = offPadLabelRect();
    const bool selected = m_pressed[static_cast<std::size_t>(offPad->button)];
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QPen(selected ? QColor(255, 255, 255, 235) : palette().color(QPalette::Light), selected ? 2.5 : 1.5));
    painter.setBrush(selected ? accent.darker(130) : palette().color(QPalette::Button));
    painter.drawRoundedRect(label, label.height() / 2, label.height() / 2);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(label, Qt::AlignCenter, offPad->text);
}

void ControllerDiagram::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF position = event->position();
    if (const auto offPad = offPadButton(); offPad && offPadLabelRect().contains(position)) {
        emit buttonClicked(offPad->button);
        return;
    }
    const QRectF image = pictureRect();
    for (const Hotspot& hotspot : hotspots()) {
        const QPointF delta = position - hotspotCentre(hotspot, image);
        const double radius = image.height() * hotspot.radius;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= radius * radius) {
            emit buttonClicked(hotspot.button);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
