#include "BindingIcons.h"

#include <QPainter>
#include <QPixmap>

namespace {

constexpr int kSize = 16;

QPixmap keyPixmap(const QColor& color)
{
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4));
    p.drawRoundedRect(QRectF(1.5, 2.5, 13, 11), 2.5, 2.5);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(4, 5.5, 8, 1.6));
    p.drawRect(QRectF(5, 9, 6, 1.6));
    return pixmap;
}

QPixmap padPixmap(const QColor& color)
{
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4));
    p.drawRoundedRect(QRectF(1, 4, 14, 8), 4, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRect(QRectF(3.5, 6.2, 3.4, 1.1)); // d-pad
    p.drawRect(QRectF(4.65, 5.05, 1.1, 3.4));
    p.drawEllipse(QPointF(10.2, 6.4), 1.0, 1.0); // buttons
    p.drawEllipse(QPointF(12.2, 8.2), 1.0, 1.0);
    return pixmap;
}

}

QIcon bindingIcon(const InputBinding& binding, const QColor& color)
{
    switch (binding.type) {
    case InputBinding::Type::Key:
        return QIcon(keyPixmap(color));
    case InputBinding::Type::JoyButton:
    case InputBinding::Type::JoyHat:
    case InputBinding::Type::JoyAxis:
        return QIcon(padPixmap(color));
    case InputBinding::Type::None:
        break;
    }
    return {};
}
