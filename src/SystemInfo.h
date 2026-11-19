#pragma once

#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

#include "PadButton.h"

enum class System { SG1000, MasterSystem, GameGear, Genesis, SegaCD, Sega32X };

// Systems whose controllers are physically alike share one set of bindings.
enum class InputGroup { MasterSystem, Genesis };

struct SystemDef {
    System id;
    QString folder;
    QStringList extensions;
    InputGroup inputGroup;
};

struct InputAction {
    PadButton button;
    QString label;
    bool sixButtonOnly = false;

    bool availableWith(PadType type) const { return !sixButtonOnly || type == PadType::SixButton; }
};

struct InputGroupDef {
    InputGroup id;
    QString key; // stable identifier used in the configuration file
    std::vector<InputAction> actions;
    bool selectablePadType = false; // the user can choose between the 3 and 6 button pad
};

const std::vector<SystemDef>& allSystems();
const SystemDef& systemDef(System id);
std::optional<System> systemForFile(const QString& path);
QString romFileDialogFilter();

const std::vector<InputGroupDef>& allInputGroups();
const InputGroupDef& inputGroupDef(InputGroup id);
// "Genesis / Sega CD / 32X": the member systems, in table order.
QString inputGroupTitle(InputGroup id);
