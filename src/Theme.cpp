#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace Theme {

namespace {

struct Colors {
    const char* name;
    const char* window;
    const char* base;
    const char* alternate;
    const char* text;
    const char* button;
    const char* highlight;
    const char* highlightedText;
};

const Colors kThemes[] = {
    {"Light", "#f0f0f0", "#ffffff", "#e8e8e8", "#1a1a1a", "#e6e6e6", "#2a82da", "#ffffff"},
    {"Dark (Gray)", "#2b2b2b", "#1f1f1f", "#262626", "#e6e6e6", "#3a3a3a", "#5a8fd8", "#ffffff"},
    {"Dark (Blue)", "#1e2430", "#151a24", "#1a202b", "#dde4f0", "#2a3345", "#3d7de0", "#ffffff"},
    {"Dark (Purple)", "#241c30", "#191322", "#1e1729", "#e6def2", "#33284a", "#9b5de5", "#ffffff"},
    {"Dark (Green)", "#1c2a22", "#131e18", "#18241d", "#dcefe3", "#27402f", "#3fb36b", "#ffffff"},
    {"AMOLED (Black)", "#000000", "#000000", "#0c0c0c", "#e0e0e0", "#161616", "#3d7de0", "#ffffff"},
    {"AMOLED (Black/Purple)", "#000000", "#050308", "#0d0814", "#e6def2", "#1a1026", "#8a3ffc", "#ffffff"},
};

constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

QPalette paletteFor(const Colors& c)
{
    QPalette palette;
    const QColor text(c.text);
    const QColor disabled = QColor(c.text).darker(160);
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, c.base);
    palette.setColor(QPalette::AlternateBase, c.alternate);
    palette.setColor(QPalette::ToolTipBase, c.base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, c.button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, c.highlight);
    palette.setColor(QPalette::Highlight, c.highlight);
    palette.setColor(QPalette::HighlightedText, c.highlightedText);
    palette.setColor(QPalette::PlaceholderText, disabled);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, disabled);
    return palette;
}

}

QStringList names()
{
    QStringList list = {"Follow System"};
    for (const Colors& colors : kThemes)
        list << colors.name;
    return list;
}

void apply(int index)
{
    static const QString systemStyle = QApplication::style()->objectName();
    static const QPalette systemPalette = QApplication::palette();
    if (index <= 0 || index > kThemeCount) {
        QApplication::setStyle(QStyleFactory::create(systemStyle));
        QApplication::setPalette(systemPalette);
        return;
    }
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(paletteFor(kThemes[index - 1]));
}

}
