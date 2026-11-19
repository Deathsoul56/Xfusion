#pragma once

#include <QPixmap>
#include <QWidget>
#include <optional>
#include <vector>

#include "SystemInfo.h"

// The pad of one input group. Genesis-family groups show a picture of the 3 or 6 button pad; the button
// of the selected binding row glows on it and clicking a button selects its row. The Master System pad is
// drawn schematically.
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

    bool usesPicture() const { return m_group == InputGroup::Genesis; }
    const QPixmap& picture() const { return m_padType == PadType::SixButton ? m_sixButton : m_threeButton; }
    QRectF pictureRect() const;
    QPointF hotspotCentre(const Hotspot& hotspot, const QRectF& picture) const;
    const std::vector<Hotspot>& hotspots() const;
    // The 6-button picture has no MODE button on its face, so it is drawn as a label above the picture.
    QRectF modeLabelRect() const;
    void paintPicture(QPainter& painter);
    void paintSchematic(QPainter& painter);

    InputGroup m_group = InputGroup::Genesis;
    PadType m_padType = PadType::SixButton;
    std::optional<PadButton> m_highlight;
    QPixmap m_threeButton;
    QPixmap m_sixButton;
};
