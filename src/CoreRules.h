#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>

#include "SystemInfo.h"

// Which libretro core plays which system. Two cores are used (ADR-0024): Genesis Plus GX for most systems, because it
// is the most precise and has the best Sega CD support, and PicoDrive for the 32X, which only it can emulate.
// This file is data and decisions only; what is particular to each core (options, button ids) lives in CoreProfile.
namespace CoreId {
inline const QString PicoDrive = "picodrive";
inline const QString GenesisPlusGx = "genesis_plus_gx";
}

struct CoreInfo {
    QString id;
    QString name; // as shown to the user
    QString libraryBaseName; // "genesis_plus_gx_libretro"
    std::vector<System> systems; // what the core can play
    bool supports(System system) const;
};

const std::vector<CoreInfo>& allCores();
const CoreInfo* coreInfo(const QString& id);

// The core each system uses unless the user chose another one.
QString defaultCoreFor(System system);

// The stable key of a system in the settings file ("sg1000", "mastersystem", "gamegear", "genesis", "segacd", "32x").
QString systemKey(System system);

struct CoreDecision {
    QString id; // empty when no installed core can play the game
    QString note; // why it is not the first choice, when it is not; empty otherwise
};

// The core for a game: the one chosen for that game (`gameChoice`), else the one chosen for the system, else the default;
// a choice is only taken when that core can play the system. A core that is not installed (`installed`) is replaced by
// another that can play the system.
CoreDecision chooseCore(System system, const QMap<QString, QString>& userChoices, const std::function<bool(const QString& coreId)>& installed,
                        const QString& gameChoice = QString());
