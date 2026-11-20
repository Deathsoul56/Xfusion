#pragma once

#include <QDialog>
#include <array>

#include "GameSaves.h"

class QComboBox;
class QPushButton;

// "Save with Preview" and "Load with Preview": ten slots of a bank shown with the picture that was on screen when
// each state was saved. Choosing a slot is reported through slotChosen(); the caller does the work.
class StatePreviewDialog : public QDialog {
    Q_OBJECT
public:
    StatePreviewDialog(const GameSaves& saves, bool saving, int bank, QWidget* parent = nullptr);

    // Reads the slots of the current bank again.
    void refresh();

signals:
    void slotChosen(int slot);

private:
    const GameSaves& m_saves;
    QComboBox* m_bank;
    std::array<QPushButton*, 10> m_buttons{};
};
