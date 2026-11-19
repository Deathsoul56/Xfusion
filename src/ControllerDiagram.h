#pragma once

#include <QWidget>
#include <optional>

#include "SystemInfo.h"

// A schematic pad for one input group; the button of the selected binding row is highlighted.
class ControllerDiagram : public QWidget {
    Q_OBJECT
public:
    explicit ControllerDiagram(QWidget* parent = nullptr);

    void setGroup(InputGroup group);
    void setHighlight(std::optional<PadButton> button);

    QSize minimumSizeHint() const override { return {420, 190}; }
    QSize sizeHint() const override { return {560, 250}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    InputGroup m_group = InputGroup::Genesis;
    std::optional<PadButton> m_highlight;
};
