#include "ScopedPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

#include "CoreRules.h"

ScopedPage::ScopedPage(const QString& section, const AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_section(section)
    , m_working(settings)
    , m_scope(new QComboBox(this))
{
    m_scope->addItem("Global (all systems)", QString());
    for (const SystemDef& def : allSystems())
        m_scope->addItem(def.folder, systemKey(def.id));
    m_scope->setToolTip("Global applies to every system. Choosing a system lets it have its own value for each setting, or follow the global one.");
    connect(m_scope, &QComboBox::currentIndexChanged, this, &ScopedPage::changeScope);
}

QWidget* ScopedPage::scopeSelector()
{
    auto* box = new QWidget(this);
    auto* layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Settings for:", box));
    layout->addWidget(m_scope, 1);
    return box;
}

QWidget* ScopedPage::row(QWidget* control, const QString& key)
{
    auto* box = new QWidget(this);
    auto* layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(control, 1);
    auto* global = new QCheckBox("Global", box);
    global->setToolTip("Use the global value. Untick it to give this system a value of its own.");
    layout->addWidget(global);
    global->hide();
    connect(global, &QCheckBox::toggled, this, [this] {
        capture();
        show(m_scopeKey);
    });
    m_rows.append({key, control, global});
    return box;
}

QWidget* ScopedPage::globalOnly(QWidget* control)
{
    m_globalOnly.append(control);
    return control;
}

void ScopedPage::start()
{
    show(QString());
}

// The controls are read into the working copy: the global values, or the overrides of the chosen system.
void ScopedPage::capture()
{
    if (!m_system) {
        readControls(m_working);
        return;
    }
    AppSettings seen = m_working;
    readControls(seen);
    const QJsonObject values = seen.toJson().value(m_section).toObject();
    QJsonObject entry = m_working.systemOverrides.value(m_scopeKey);
    QJsonObject section = entry.value(m_section).toObject();
    for (const Row& row : m_rows) {
        if (row.global->isChecked())
            section.remove(row.key);
        else
            section[row.key] = values.value(row.key);
    }
    if (section.isEmpty())
        entry.remove(m_section);
    else
        entry[m_section] = section;
    if (entry.isEmpty())
        m_working.systemOverrides.remove(m_scopeKey);
    else
        m_working.systemOverrides[m_scopeKey] = entry;
}

void ScopedPage::show(const QString& systemKeyText)
{
    m_scopeKey = systemKeyText;
    m_system.reset();
    for (const SystemDef& def : allSystems())
        if (!systemKeyText.isEmpty() && systemKey(def.id) == systemKeyText)
            m_system = def.id;
    loadControls(m_system ? m_working.forSystem(*m_system) : m_working);
    for (QWidget* control : m_globalOnly)
        control->setEnabled(!m_system);
    for (const Row& row : m_rows) {
        const bool follows = m_system && !m_working.overrides(*m_system, m_section, row.key);
        row.global->blockSignals(true);
        row.global->setVisible(m_system.has_value());
        row.global->setChecked(follows);
        row.global->blockSignals(false);
        row.control->setEnabled(!follows);
    }
}

void ScopedPage::changeScope()
{
    capture();
    show(m_scope->currentData().toString());
}

void ScopedPage::applyTo(AppSettings& settings)
{
    capture();
    // The global values go through the controls, which read them back into `settings`; then the view of the chosen
    // system is put back.
    loadControls(m_working);
    readControls(settings);
    for (const SystemDef& def : allSystems()) {
        const QString key = systemKey(def.id);
        const QJsonObject mine = m_working.systemOverrides.value(key).value(m_section).toObject();
        QJsonObject entry = settings.systemOverrides.value(key);
        if (mine.isEmpty())
            entry.remove(m_section);
        else
            entry[m_section] = mine;
        if (entry.isEmpty())
            settings.systemOverrides.remove(key);
        else
            settings.systemOverrides[key] = entry;
    }
    show(m_scopeKey);
}

void ScopedPage::restoreDefaults()
{
    if (!m_system) {
        loadControls(AppSettings());
        return;
    }
    QJsonObject entry = m_working.systemOverrides.value(m_scopeKey);
    entry.remove(m_section);
    if (entry.isEmpty())
        m_working.systemOverrides.remove(m_scopeKey);
    else
        m_working.systemOverrides[m_scopeKey] = entry;
    show(m_scopeKey);
}
