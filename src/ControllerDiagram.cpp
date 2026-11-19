#include "ControllerDiagram.h"

#include <QPainter>
#include <QPainterPath>
#include <vector>

namespace {

// Positions are fractions of the pad body so the drawing scales with the widget.
struct Spot {
    PadButton button;
    double x;
    double y;
    double radius; // fraction of the body height
    const char* text;
};

const std::vector<Spot>& spots(InputGroup group)
{
    static const std::vector<Spot> genesis = {
        {PadButton::Up, 0.20, 0.30, 0.075, ""},
        {PadButton::Down, 0.20, 0.70, 0.075, ""},
        {PadButton::Left, 0.11, 0.50, 0.075, ""},
        {PadButton::Right, 0.29, 0.50, 0.075, ""},
        {PadButton::X, 0.60, 0.30, 0.085, "X"},
        {PadButton::Y, 0.72, 0.30, 0.085, "Y"},
        {PadButton::Z, 0.84, 0.30, 0.085, "Z"},
        {PadButton::A, 0.60, 0.62, 0.105, "A"},
        {PadButton::B, 0.72, 0.62, 0.105, "B"},
        {PadButton::C, 0.84, 0.62, 0.105, "C"},
        {PadButton::Mode, 0.43, 0.30, 0.06, "MODE"},
        {PadButton::Start, 0.43, 0.70, 0.06, "START"},
    };
    static const std::vector<Spot> masterSystem = {
        {PadButton::Up, 0.20, 0.30, 0.085, ""},
        {PadButton::Down, 0.20, 0.70, 0.085, ""},
        {PadButton::Left, 0.10, 0.50, 0.085, ""},
        {PadButton::Right, 0.30, 0.50, 0.085, ""},
        {PadButton::B, 0.64, 0.58, 0.14, "1"},
        {PadButton::C, 0.84, 0.46, 0.14, "2"},
        {PadButton::Start, 0.44, 0.72, 0.06, "START"},
    };
    return group == InputGroup::Genesis ? genesis : masterSystem;
}

bool isDirection(PadButton button)
{
    return button == PadButton::Up || button == PadButton::Down || button == PadButton::Left || button == PadButton::Right;
}

}

ControllerDiagram::ControllerDiagram(QWidget* parent)
    : QWidget(parent)
{
}

void ControllerDiagram::setGroup(InputGroup group)
{
    m_group = group;
    update();
}

void ControllerDiagram::setHighlight(std::optional<PadButton> button)
{
    m_highlight = button;
    update();
}

void ControllerDiagram::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

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

    for (const Spot& spot : spots(m_group)) {
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

        const bool pill = spot.button == PadButton::Start || spot.button == PadButton::Mode;
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
