#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace Theme {

namespace {

struct Colors {
    const char* name;
    const char* window;
    const char* panel;
    const char* alternate;
    const char* elevated;
    const char* border;
    const char* borderLight;
    bool light;
    const char* accent;
    const char* accentLight;
    const char* accentLighter;
    const char* accentDark;
};

// The order is stored in the settings file as an index, so new themes are only ever appended. It is the same list,
// in the same order, as snes9xrd.
const Colors kThemes[] = {
    {"Light", "#f3f3f4", "#e8e8ea", "#ececee", "#dedee1", "#cfcfd3", "#b7b7bd", true, "#2f6fed", "#4a84f0", "#7aa5f5", "#1a56c4"},
    {"Dark Fusion (Blue)", "#1e1f25", "#16171c", "#1a1b21", "#23252c", "#2a2c33", "#474b58", false, "#4a90ff", "#4a9cff", "#6aa6ff", "#3a4d6e"},
    {"Dark (Purple)", "#211a2e", "#170f22", "#1c1528", "#2c2140", "#3d2c56", "#474b58", false, "#b06bff", "#c085ff", "#d4aaff", "#4a2e70"},
    {"Dark (Green)", "#14231c", "#0d1a14", "#112018", "#1c3327", "#24503a", "#474b58", false, "#35d98f", "#5ce8a6", "#8cf0c0", "#1f5c3f"},
    {"Dark (Red)", "#2c141d", "#200e15", "#271219", "#3d1b26", "#5c2536", "#474b58", false, "#ff4f6b", "#ff7086", "#ff9aab", "#6e1f30"},
    {"Dark (Teal)", "#10262a", "#0a1c1f", "#0f2226", "#1a3a3e", "#235055", "#474b58", false, "#2fd0d1", "#57dede", "#8ceaea", "#1c565a"},
    {"Dark (Orange)", "#2a1d10", "#1f150a", "#26190d", "#3d2a15", "#5c4020", "#474b58", false, "#ff9a3c", "#ffb066", "#ffc98f", "#6e4420"},
    {"Dark Fusion (Gray)", "#202124", "#18191b", "#1c1d20", "#2a2b2e", "#34353a", "#4a4b52", false, "#6a90c9", "#82a3d6", "#a3bfe6", "#35506e"},
    {"Grey Matter (Gray)", "#232323", "#1a1a1a", "#1f1f1f", "#2e2e2e", "#3a3a3a", "#505050", false, "#9a9a9a", "#b5b5b5", "#cfcfcf", "#3a3a3a"},
    {"Untouched Lagoon (Grayish Green/Blue)", "#eef2f0", "#e2e9e6", "#e7ece9", "#d7e0dc", "#c3d0cb", "#a9bab3", true, "#3f8f8a", "#5aa8a2", "#82c4bd", "#1f5350"},
    {"Baby Pastel (Pink)", "#fdf1f5", "#fbe4ec", "#fce8ef", "#f8d7e3", "#f0c2d4", "#e3a8c1", true, "#e0609a", "#ea7fae", "#f2a3c7", "#a6396d"},
    {"Pizza Time! (Brown-ish/Creamy White)", "#fbf3e6", "#f3e6cf", "#f7ecdb", "#ecdcbd", "#ddc79c", "#c9ac7c", true, "#c2703a", "#d3894f", "#e3aa78", "#8a4b21"},
    {"Blue Cloud (White/Blue)", "#f5f7fb", "#e9eef7", "#eef2f9", "#dde5f2", "#c7d3e6", "#aebdd6", true, "#2f6fed", "#4a84f0", "#7aa5f5", "#1a56c4"},
    {"Scarlet Devil (Red/Purple)", "#24141f", "#190d16", "#1f111b", "#33202c", "#4d2c40", "#64384f", false, "#d6386f", "#e35a8a", "#ef85ac", "#7a1f45"},
    {"Violet Angel (Blue/Purple)", "#1a1830", "#121022", "#171528", "#262143", "#383060", "#4c4380", false, "#7c6bff", "#9585ff", "#b6acff", "#3d3480"},
    {"Cobalt Sky (Blue)", "#0f1c2e", "#0a1420", "#0d1826", "#16283f", "#1f3a56", "#2c4d6e", false, "#2f8fe0", "#4fa3ea", "#7fc0f2", "#1a5085"},
    {"AMOLED (Black)", "#000000", "#000000", "#060606", "#121212", "#202020", "#333333", false, "#3d8bff", "#5c9fff", "#8ebeff", "#1c4a86"},
    {"AMOLED (Black/Purple)", "#000000", "#000000", "#060606", "#121212", "#202020", "#333333", false, "#b06bff", "#c085ff", "#d4aaff", "#4a2e70"},
    {"Ruby (Black/Red)", "#120608", "#0a0203", "#0f0507", "#1e0b0e", "#331216", "#4a1a20", false, "#e8384f", "#ee5c70", "#f38b98", "#7a1a26"},
    {"Sapphire (Black/Blue)", "#050a12", "#02060b", "#04080f", "#0c1a29", "#142943", "#1d3a5c", false, "#2f7fe0", "#4f97ea", "#7fb8f2", "#163f70"},
    {"Emerald (Black/Green)", "#051209", "#020a05", "#040e08", "#0c2013", "#143a22", "#1c4e2e", false, "#2fd177", "#52dc8f", "#82e8ae", "#17603a"},
};

constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
// Positions in the list shown to the user, where 0 is "Follow System".
constexpr int kLightIndex = 1;
constexpr int kDarkBlueIndex = 2;

struct Text {
    const char* normal;
    const char* bright;
    const char* muted;
    const char* disabled;
};

constexpr Text kDarkText = {"#e2e2e2", "#f5f5f5", "#9a9dab", "#555555"};
constexpr Text kLightText = {"#202124", "#101012", "#5f6368", "#9aa0a6"};

QPalette paletteFor(const Colors& c)
{
    const Text& t = c.light ? kLightText : kDarkText;
    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, t.bright);
    palette.setColor(QPalette::Base, c.panel);
    palette.setColor(QPalette::AlternateBase, c.alternate);
    palette.setColor(QPalette::ToolTipBase, c.panel);
    palette.setColor(QPalette::ToolTipText, t.normal);
    palette.setColor(QPalette::Text, t.normal);
    palette.setColor(QPalette::Button, c.elevated);
    palette.setColor(QPalette::ButtonText, t.bright);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, c.accent);
    palette.setColor(QPalette::Highlight, c.accentDark);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, t.muted);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, t.disabled);
    return palette;
}

QString styleSheetFor(const Colors& c)
{
    QFile file(":/theme.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString qss = QString::fromUtf8(file.readAll());
    const Text& t = c.light ? kLightText : kDarkText;
    // No token is a prefix of another one (they end in @), so the order does not matter.
    const std::pair<const char*, const char*> tokens[] = {
        {"@BG_WINDOW@", c.window}, {"@BG_PANEL@", c.panel}, {"@BG_ALT@", c.alternate}, {"@BG_ELEVATED@", c.elevated},
        {"@BORDER_LIGHT@", c.borderLight}, {"@BORDER@", c.border}, {"@TEXT_BRIGHT@", t.bright}, {"@TEXT_MUTED@", t.muted},
        {"@TEXT_DISABLED@", t.disabled}, {"@TEXT@", t.normal}, {"@ACCENT_LIGHTER@", c.accentLighter}, {"@ACCENT_LIGHT@", c.accentLight},
        {"@ACCENT_DARK@", c.accentDark}, {"@ACCENT@", c.accent},
        {"@SHADE@", c.light ? "black" : "white"}, // which variant of the arrow and tick images reads on this theme
    };
    for (const auto& [token, value] : tokens)
        qss.replace(QLatin1String(token), QLatin1String(value));
    return qss;
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
    // "Follow System" picks between our own Light and Dark Fusion (Blue), so the application never falls back to
    // the unstyled look.
    if (index <= 0 || index > kThemeCount) {
        const bool systemIsDark = QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        index = systemIsDark ? kDarkBlueIndex : kLightIndex;
    }
    const Colors& colors = kThemes[index - 1];
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(paletteFor(colors));
    qApp->setStyleSheet(styleSheetFor(colors));
}

}
