#pragma once

#include <QElapsedTimer>
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
    void post(const QString& text);
    int messageCount() const { return static_cast<int>(m_messages.size()); }
    QStringList messageTexts() const;
    void clearMessages();

    void setFps(double fps);
    void setIndicators(bool paused, bool fastForward, bool rewinding);
    void setPressedText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Message {
        QString text;
        QElapsedTimer age;
    };

    void expire();
    void drawBox(QPainter& painter, const QString& text, const QRectF& area, Qt::Alignment alignment, int& offset, bool fromBottom);

    Options m_options;
    std::deque<Message> m_messages;
    QTimer m_timer;
    double m_fps = 0;
    bool m_paused = false;
    bool m_fastForward = false;
    bool m_rewinding = false;
    QString m_pressed;
};
