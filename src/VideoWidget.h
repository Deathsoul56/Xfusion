#pragma once

#include <QImage>
#include <QWidget>

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    void setFrame(const QImage& frame, double aspectRatio);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_frame;
    double m_aspectRatio = 4.0 / 3.0;
};
