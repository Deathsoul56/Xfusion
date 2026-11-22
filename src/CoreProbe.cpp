#include "CoreProbe.h"

#include <QLibrary>

#include "libretro.h"

bool probeCore(const QString& libraryPath, QString* name, QString* version, QString* error)
{
    QLibrary library(libraryPath);
    if (!library.load()) {
        *error = QString("The file is not a library this system can load: %1").arg(library.errorString());
        return false;
    }
    using ApiVersion = unsigned (*)();
    using SystemInfo = void (*)(retro_system_info*);
    const auto apiVersion = reinterpret_cast<ApiVersion>(library.resolve("retro_api_version"));
    const auto systemInfo = reinterpret_cast<SystemInfo>(library.resolve("retro_get_system_info"));
    if (!apiVersion || !systemInfo) {
        *error = "The file is not a libretro core";
        library.unload();
        return false;
    }
    if (apiVersion() != RETRO_API_VERSION) {
        *error = QString("The core uses libretro API %1, not %2").arg(apiVersion()).arg(RETRO_API_VERSION);
        library.unload();
        return false;
    }
    retro_system_info info{};
    systemInfo(&info);
    *name = QString::fromUtf8(info.library_name);
    *version = QString::fromUtf8(info.library_version);
    library.unload();
    return true;
}
