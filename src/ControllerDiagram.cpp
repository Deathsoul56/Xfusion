#include "ControllerDiagram.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace {

constexpr double kMargin = 10.0;
constexpr double kLabelStrip = 30.0; // room above the picture for the MODE label

struct Spot {
    PadButton button;
    double x;
    double y;
    double radius; // fraction of the body height
    const char* text;
};

const std::vector<Spot>& schematicSpots()
{
    static const std::vector<Spot> spots = {
        {PadButton::Up, 0.20, 0.30, 0.085, ""},
        {PadButton::Down, 0.20, 0.70, 0.085, ""},
        {PadButton::Left, 0.10, 0.50, 0.085, ""},
        {PadButton::Right, 0.30, 0.50, 0.085, ""},
        {PadButton::B, 0.64, 0.58, 0.14, "1"},
        {PadButton::C, 0.84, 0.46, 0.14, "2"},
        {PadButton::Start, 0.44, 0.72, 0.06, "START"},
    };
    return spots;
}

bool isDirection(PadButton button)
{
    return button == PadButton::Up || button == PadButton::Down || button == PadButton::Left || button == PadButton::Right;
}

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

void ControllerDiagram::setHighlight(std::optional<PadButton> button)
{
    m_highlight = button;
    update();
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
    return m_padType == PadType::SixButton ? six : three;
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

QRectF ControllerDiagram::modeLabelRect() const
{
    const QRectF image = pictureRect();
    const double width = 64;
    const double height = 22;
    return {image.center().x() - width / 2, image.top() - height - 6, width, height};
}

void ControllerDiagram::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (usesPicture())
        paintPicture(painter);
    else
        paintSchematic(painter);
}

void ControllerDiagram::paintPicture(QPainter& painter)
{
    const QRectF image = pictureRect();
    painter.drawPixmap(image, picture(), QRectF(picture().rect()));

    const QColor accent = palette().color(QPalette::Highlight);
    if (m_highlight) {
        for (const Hotspot& hotspot : hotspots()) {
            if (hotspot.button != *m_highlight)
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
    }

    if (m_padType != PadType::SixButton)
        return;
    const QRectF label = modeLabelRect();
    const bool selected = m_highlight && *m_highlight == PadButton::Mode;
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QPen(selected ? QColor(255, 255, 255, 235) : palette().color(QPalette::Light), selected ? 2.5 : 1.5));
    painter.setBrush(selected ? accent.darker(130) : palette().color(QPalette::Button));
    painter.drawRoundedRect(label, label.height() / 2, label.height() / 2);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(label, Qt::AlignCenter, "MODE");
}

void ControllerDiagram::paintSchematic(QPainter& painter)
{
    const QPalette& colors = palette();
    const QColor body = colors.color(QPalette::Mid);
    const QColor outline = colors.color(QPalette::Light);
    const QColor button = colors.color(QPalette::Button).lighter(140);
    const QColor accent = colors.color(QPalette::Highlight);
    const QColor text = colors.color(QPalette::WindowText);

    const double width = qMin(this->width() - 20.0, (this->height() - 20.0) * 2.4);
    const double height = width / 2.4;
    const QRectF pad((this->width() - width) / 2, (this->height() - height) / 2, width, height);

    painter.setPen(QPen(outline, 2));
    painter.setBrush(body);
    painter.drawRoundedRect(pad, height * 0.28, height * 0.28);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qMax(8, int(height * 0.075)));
    painter.setFont(font);

    // The direction pad is one cross; each arm is highlighted on its own.
    const double armHalf = height * 0.075;
    const QPointF center(pad.left() + pad.width() * 0.20, pad.top() + pad.height() * 0.50);
    QPainterPath cross;
    const double reach = pad.width() * 0.10;
    cross.addRoundedRect(QRectF(center.x() - reach, center.y() - armHalf, reach * 2, armHalf * 2), 4, 4);
    cross.addRoundedRect(QRectF(center.x() - armHalf, center.y() - reach, armHalf * 2, reach * 2), 4, 4);
    painter.setPen(QPen(outline, 1.5));
    painter.setBrush(colors.color(QPalette::Dark));
    painter.drawPath(cross.simplified());

    for (const Spot& spot : schematicSpots()) {
        const QPointF position(pad.left() + pad.width() * spot.x, pad.top() + pad.height() * spot.y);
        const double radius = pad.height() * spot.radius;
        const bool highlighted = m_highlight && *m_highlight == spot.button;

        if (isDirection(spot.button)) {
            if (highlighted) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(accent);
                painter.drawEllipse(position, armHalf * 0.9, armHalf * 0.9);
            }
            continue;
        }

        const bool pill = spot.button == PadButton::Start;
        painter.setPen(QPen(highlighted ? accent : outline, highlighted ? 3 : 1.5));
        painter.setBrush(highlighted ? accent.darker(160) : button);
        if (pill) {
            const QFontMetricsF metrics(painter.font());
            const double pillWidth = metrics.horizontalAdvance(spot.text) + radius * 1.4;
            const double pillHeight = metrics.height() + radius * 0.5;
            const QRectF rect(position.x() - pillWidth / 2, position.y() - pillHeight / 2, pillWidth, pillHeight);
            painter.drawRoundedRect(rect, pillHeight * 0.5, pillHeight * 0.5);
            painter.setPen(text);
            painter.drawText(rect, Qt::AlignCenter, spot.text);
        } else {
            painter.drawEllipse(position, radius, radius);
            painter.setPen(text);
            painter.drawText(QRectF(position.x() - radius, position.y() - radius, radius * 2, radius * 2), Qt::AlignCenter, spot.text);
        }
    }
}

void ControllerDiagram::mousePressEvent(QMouseEvent* event)
{
    if (!usesPicture() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF position = event->position();
    if (m_padType == PadType::SixButton && modeLabelRect().contains(position)) {
        emit buttonClicked(PadButton::Mode);
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
