#pragma once

#include <QImage>
#include <QWidget>
#include <mutex>

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    // Safe to call from any thread. The image is copied before the call returns.
    void submitFrame(const QImage& frame, double aspectRatio);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void takeReadyFrame();

    // Three buffers rotate so the emulation thread and the GUI thread never share one:
    // m_back is written by the producer, m_ready is handed over under the mutex, m_front is painted.
    QImage m_back;
    QImage m_ready;
    QImage m_front;
    double m_readyAspect = 4.0 / 3.0;
    double m_aspectRatio = 4.0 / 3.0;
    bool m_updateQueued = false;
    std::mutex m_mutex;
};
