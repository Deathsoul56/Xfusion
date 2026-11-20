#pragma once

#include <QIcon>

#include "InputBinding.h"

// Small icons drawn at run time: a keyboard key for key bindings and a gamepad for everything a controller sends.
// They follow the text colour of the palette so they read on any theme.
QIcon bindingIcon(const InputBinding& binding, const QColor& color);
