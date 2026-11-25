#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <deque>

#include "AppSettings.h"

// Transparent layer over the picture: stacked messages (at most four, newest nearest the chosen corner) and the
// optional overlays (frame rate, pause / fast-forward / rewind icons, clock, pressed buttons).
class OsdOverlay : public QWidget {
    Q_OBJECT
public:
    struct Options {
        MessageMode messages = MessageMode::Onscreen;
        int size = 24;
        OsdCorner corner = OsdCorner::BottomLeft;
        bool showFps = false;
        bool showIndicators = true;
        bool showTime = false;
        bool showPressed = false;
    };

    static constexpr int kMaxMessages = 4;
    static constexpr int kMessageMs = 2000;

    explicit OsdOverlay(QWidget* parent = nullptr);

    void setOptions(const Options& options);
    // The message list and its texts, newest last; for tests and the status bar.
    // `durationMs` of 0 is the usual time a message stays.
    // `icon`, when it is not null, is drawn at the left of the message (the badge of an achievement).
    void post(const QString& text, int durationMs = 0, const QImage& icon = QImage());

    void setFps(double fps);
    void setIndicators(bool paused, bool fastForward, bool rewinding);
    void setPressedText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Message {
        QString text;
        QElapsedTimer age;
        int lifeMs = 0;
        QImage icon;
    };

    void expire();
    void drawBox(QPainter& painter, const QString& text, const QRectF& area, Qt::Alignment alignment, int& offset, bool fromBottom, const QImage& icon = QImage());

    Options m_options;
    std::deque<Message> m_messages;
    QTimer m_timer;
    double m_fps = 0;
    bool m_paused = false;
    bool m_fastForward = false;
    bool m_rewinding = false;
    QString m_pressed;
};
