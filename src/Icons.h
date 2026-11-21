#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

#include "PadButton.h"

class QAction;
class QWidget;

// Monochrome icons from the Material set (src/resources/icons, Apache-2.0). They are recoloured with the text colour of
// the current palette, so one set serves every theme; call refreshThemedIcons() after the theme changes.
QIcon themedIcon(const QString& name, const QColor& color);
QIcon themedIcon(const QString& name);

// Gives an action an icon and remembers its name so that refreshThemedIcons() can recolour it.
void setThemedActionIcon(QAction* action, const QString& name);
// Recolours every action under `root` that got its icon from setThemedActionIcon().
void refreshThemedIcons(QWidget* root);

// The icon of a pad button for the row of a table: an arrow for the d-pad, a lettered disc for the others.
QIcon padButtonIcon(PadButton button, const QColor& color);
