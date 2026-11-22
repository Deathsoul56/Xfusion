#include "CoreRules.h"

bool CoreInfo::supports(System system) const
{
    for (System candidate : systems)
        if (candidate == system)
            return true;
    return false;
}

const std::vector<CoreInfo>& allCores()
{
    static const std::vector<CoreInfo> cores = {
        {CoreId::GenesisPlusGx, "Genesis Plus GX", "genesis_plus_gx_libretro",
         {System::SG1000, System::MasterSystem, System::GameGear, System::Genesis, System::SegaCD}},
        {CoreId::PicoDrive, "PicoDrive", "picodrive_libretro",
         {System::SG1000, System::MasterSystem, System::GameGear, System::Genesis, System::SegaCD, System::Sega32X}},
    };
    return cores;
}

const CoreInfo* coreInfo(const QString& id)
{
    for (const CoreInfo& core : allCores())
        if (core.id == id)
            return &core;
    return nullptr;
}

QString defaultCoreFor(System system)
{
    return system == System::Sega32X ? CoreId::PicoDrive : CoreId::GenesisPlusGx;
}

QString systemKey(System system)
{
    switch (system) {
    case System::SG1000: return "sg1000";
    case System::MasterSystem: return "mastersystem";
    case System::GameGear: return "gamegear";
    case System::Genesis: return "genesis";
    case System::SegaCD: return "segacd";
    case System::Sega32X: return "32x";
    }
    return "genesis";
}

CoreDecision chooseCore(System system, const QMap<QString, QString>& userChoices, const std::function<bool(const QString&)>& installed,
                        const QString& gameChoice)
{
    QString wanted = defaultCoreFor(system);
    for (const QString& chosen : {userChoices.value(systemKey(system)), gameChoice}) {
        if (!chosen.isEmpty() && coreInfo(chosen) && coreInfo(chosen)->supports(system))
            wanted = chosen;
    }

    if (installed(wanted))
        return {wanted, {}};
    // Not installed: any other core that can play the system.
    for (const CoreInfo& core : allCores())
        if (core.id != wanted && core.supports(system) && installed(core.id))
            return {core.id, QString("%1 is not installed").arg(coreInfo(wanted) ? coreInfo(wanted)->name : wanted)};
    return {};
}
