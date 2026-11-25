#pragma once

#include <QVector>
#include <QWidget>

#include "AppSettings.h"

class QCheckBox;
class QComboBox;

// The base of the Display and Sound pages (ADR-0031): the settings can be set for all systems at once ("Global") or for
// one system. With a system chosen, each setting has a "Global" box: ticked, the system follows the global value
// (shown, and not editable); unticked, it has a value of its own. For an on/off setting that makes three states: on,
// off and the global one. The page keeps its own copy of the settings while it is open and writes them back on OK.
class ScopedPage : public QWidget {
    Q_OBJECT
public:
    // Writes the global values to `settings` and the overrides of this page's section ("display" or "sound").
    void applyTo(AppSettings& settings);
    // Global: the defaults. A system: back to following the global values.
    void restoreDefaults();

protected:
    ScopedPage(const QString& section, const AppSettings& settings, QWidget* parent);
    // Call at the end of the constructor of the page, once its controls exist.
    void start();
    // The "Settings for:" selector, for the top of the page.
    QWidget* scopeSelector();
    // `control` with the "Global" box beside it, for a row of the page; `key` is its key in the section of the file.
    QWidget* row(QWidget* control, const QString& key);
    // A control that is only global (the video driver): it is not editable while a system is chosen.
    QWidget* globalOnly(QWidget* control);
    // The page's controls from the settings, and back. The value of every control is read, overridden or not.
    virtual void loadControls(const AppSettings& settings) = 0;
    virtual void readControls(AppSettings& settings) const = 0;

private:
    void capture();
    void show(const QString& systemKey);
    void changeScope();

    struct Row {
        QString key;
        QWidget* control;
        QCheckBox* global;
    };
    QString m_section;
    AppSettings m_working;
    QComboBox* m_scope;
    QString m_scopeKey; // empty for the global settings
    std::optional<System> m_system;
    QVector<Row> m_rows;
    QVector<QWidget*> m_globalOnly;
};
