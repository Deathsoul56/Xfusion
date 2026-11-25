#pragma once

#include <QImage>
#include <QWidget>
#include <mutex>

#include "AppSettings.h"
#include "DisplayLayout.h"
#include "PresetProgram.h"

class GpuCanvas;
class OsdOverlay;

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    // Safe to call from any thread. The image is copied before the call returns.
    void submitFrame(const QImage& frame, double aspectRatio);
    void clear();
    void setDisplay(const DisplayOptions& options, bool smooth, VideoFilter filter, VideoShader shader);
    // Chooses how the picture is drawn, once, before the window is shown. Software draws it here; the others use a GPU
    // canvas (through Qt's RHI) and fall back to software, with a message, if the GPU cannot be used. Later calls do nothing:
    // the window is set up for one driver for as long as it lives.
    void setDriver(VideoDriver driver);
    // The picture goes through a shader preset (null: the built-in shader of setDisplay). Only the GPU does this;
    // `values` are the parameters the user has changed. If the GPU cannot build it, the widget says so over the picture.
    void setShaderPreset(std::shared_ptr<const PresetProgram> program, const QMap<QString, double>& values);
    // The parameters of the preset change (a dialog, live); the preset itself stays.
    void setShaderParameters(const QMap<QString, double>& values);
    // Whether the GPU is drawing the picture.
    bool usesGpu() const { return m_gpu != nullptr; }

    OsdOverlay* osd() const { return m_osd; }
    // The last frame the core produced, unfiltered (null before the first one). GUI thread only.
    QImage currentFrame() const { return m_front; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void takeReadyFrame();
    void refilter();
    void updateGpuView();

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
    VideoShader m_shader = VideoShader::None;
    GpuCanvas* m_gpu = nullptr;
    bool m_driverChosen = false;
    bool m_updateQueued = false;
    std::mutex m_mutex;
    OsdOverlay* m_osd;
};
