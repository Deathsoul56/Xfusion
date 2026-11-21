#pragma once

#include <QIcon>

#include "InputBinding.h"

// A keyboard for key bindings and a gamepad for everything a controller sends, in the colour given so they read on any theme.
QIcon bindingIcon(const InputBinding& binding, const QColor& color);
