#pragma once

#include <QPixmap>
#include <QWidget>
#include <optional>
#include <vector>

#include "SystemInfo.h"

// The pad of one input group: a picture of the pad where the button of the selected binding row glows and
// clicking a button selects its row. Buttons that are not on the face of the pad (MODE on the 6 button
// pad, PAUSE on the Master System console) are drawn as a label above the picture.
class ControllerDiagram : public QWidget {
    Q_OBJECT
public:
    explicit ControllerDiagram(QWidget* parent = nullptr);

    void setGroup(InputGroup group);
    void setPadType(PadType type);
    void setHighlight(std::optional<PadButton> button);

    QSize minimumSizeHint() const override { return {420, 210}; }
    QSize sizeHint() const override { return {600, 290}; }

signals:
    void buttonClicked(PadButton button);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct Hotspot {
        PadButton button;
        double x; // centre, as a fraction of the picture width
        double y; // centre, as a fraction of the picture height
        double radius; // fraction of the picture height
    };

    struct OffPadButton {
        PadButton button;
        const char* text;
    };

    const QPixmap& picture() const;
    QRectF pictureRect() const;
    QPointF hotspotCentre(const Hotspot& hotspot, const QRectF& picture) const;
    const std::vector<Hotspot>& hotspots() const;
    std::optional<OffPadButton> offPadButton() const;
    QRectF offPadLabelRect() const;

    InputGroup m_group = InputGroup::Genesis;
    PadType m_padType = PadType::SixButton;
    std::optional<PadButton> m_highlight;
    QPixmap m_threeButton;
    QPixmap m_sixButton;
    QPixmap m_masterSystem;
};
