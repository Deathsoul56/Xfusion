#pragma once

#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

enum class System { SG1000, MasterSystem, GameGear, Genesis, SegaCD, Sega32X };

struct SystemDef {
    System id;
    QString folder;
    QStringList extensions;
};

const std::vector<SystemDef>& allSystems();
const SystemDef& systemDef(System id);
std::optional<System> systemForFile(const QString& path);
QString romFileDialogFilter();
