#include "Icons.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <QWidget>

namespace {

constexpr const char* kIconProperty = "xfusionIcon";
constexpr int kSizes[] = {16, 24, 32, 48};

QPixmap render(QSvgRenderer& renderer, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return QPixmap::fromImage(image);
}

QPixmap letterDisc(const QString& letter, const QColor& color, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 16.0, size / 16.0);
    p.setPen(QPen(color, 1.3));
    p.drawEllipse(QRectF(1.5, 1.5, 13, 13));
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(8);
    p.setFont(font);
    p.drawText(QRectF(0, 0, 16, 16), Qt::AlignCenter, letter);
    return pixmap;
}

}

QIcon themedIcon(const QString& name, const QColor& color)
{
    QFile file(":/icons/" + name + ".svg");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    // The Material files have no fill of their own, so a fill on the root element reaches every path.
    QByteArray svg = file.readAll();
    svg.replace("<svg ", "<svg fill=\"" + color.name().toLatin1() + "\" ");
    QSvgRenderer renderer(svg);
    QIcon icon;
    for (int size : kSizes)
        icon.addPixmap(render(renderer, size));
    return icon;
}

QIcon themedIcon(const QString& name)
{
    return themedIcon(name, QApplication::palette().color(QPalette::WindowText));
}

void setThemedActionIcon(QAction* action, const QString& name)
{
    action->setProperty(kIconProperty, name);
    action->setIcon(themedIcon(name));
}

void refreshThemedIcons(QWidget* root)
{
    for (QAction* action : root->findChildren<QAction*>()) {
        const QString name = action->property(kIconProperty).toString();
        if (!name.isEmpty())
            action->setIcon(themedIcon(name));
    }
}

QIcon padButtonIcon(PadButton button, const QColor& color)
{
    if (isTurbo(button))
        return themedIcon("turbo", color);
    switch (button) {
    case PadButton::Up: return themedIcon("up", color);
    case PadButton::Down: return themedIcon("down", color);
    case PadButton::Left: return themedIcon("left", color);
    case PadButton::Right: return themedIcon("right", color);
    case PadButton::Start: return themedIcon("play", color);
    default: break;
    }
    static const char* const letters[] = {"", "", "", "", "A", "B", "C", "X", "Y", "Z", "", "M"};
    QIcon icon;
    for (int size : kSizes)
        icon.addPixmap(letterDisc(letters[static_cast<int>(button)], color, size));
    return icon;
}
