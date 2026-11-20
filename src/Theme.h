#pragma once

#include <QStringList>

// Colour themes for the whole application. Index 0 follows the system; the rest are palettes of our own on the
// Fusion style.
namespace Theme {

QStringList names();
// Call once the QApplication exists; safe to call again to switch.
void apply(int index);

}
