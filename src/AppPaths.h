#pragma once

#include "SystemInfo.h"

#include <QString>

namespace AppPaths {

QString dataRoot();
QString savesDir(System system);
QString systemDir();
QString coresDir();
QString configDir();
QString inputConfigPath();
QString defaultCorePath();

}
