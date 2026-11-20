#pragma once

#include <QImage>
#include <QWidget>
#include <mutex>

#include "AppSettings.h"
#include "DisplayLayout.h"

class OsdOverlay;

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    // Safe to call from any thread. The image is copied before the call returns.
    void submitFrame(const QImage& frame, double aspectRatio);
    void clear();
    void setDisplay(const DisplayOptions& options, bool smooth, VideoFilter filter);

    OsdOverlay* osd() const { return m_osd; }
    // The last frame the core produced, unfiltered (null before the first one). GUI thread only.
    QImage currentFrame() const { return m_front; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void takeReadyFrame();
    void refilter();

    // Three buffers rotate so the emulation thread and the GUI thread never share one:
    // m_back is written by the producer, m_ready is handed over under the mutex, m_front is painted.
    QImage m_back;
    QImage m_ready;
    QImage m_front;
    QImage m_shown; // m_front after the filter; this is what gets painted
    double m_readyAspect = 4.0 / 3.0;
    double m_aspectRatio = 4.0 / 3.0;
    DisplayOptions m_options;
    VideoFilter m_filter = VideoFilter::None;
    bool m_smooth = true;
    bool m_updateQueued = false;
    std::mutex m_mutex;
    OsdOverlay* m_osd;
};
