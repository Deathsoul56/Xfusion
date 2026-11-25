#include "MainWindow.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "Achievements.h"
#include "StatePreviewDialog.h"
#include "StateFile.h"

namespace {

constexpr qint64 kOopsAfterMs = 300'000; // an automatic snapshot before a reset when nothing was saved for this long
constexpr int kThumbnailWidth = 160;

}

// ------------------------------------------------------------------------------------------------- states

bool MainWindow::stateOperationsAvailable() const
{
    return m_emu && m_saves && m_gameSystem && m_statesSupported;
}

QString MainWindow::slotDescription(int slot) const
{
    if (!m_saves)
        return QString("Current slot: %1").arg(slot);
    return QString("Current slot: %1 [%2]").arg(slot).arg(QFile::exists(m_saves->statePath(slot)) ? "used" : "empty");
}

void MainWindow::setCurrentSlot(int slot)
{
    const int count = GameSaves::kMaxSlot + 1;
    m_currentSlot = ((slot % count) + count) % count;
    showMessage(slotDescription(m_currentSlot));
}

bool MainWindow::captureState(QByteArray* state, QString* error, QByteArray* achievements)
{
    if (!m_emu)
        return false;
    bool ok = false;
    // The progress of the achievements is taken at the same moment as the state, between the same two frames.
    m_emu->withCore([&] {
        ok = m_core.serialize(state, error);
        if (achievements)
            *achievements = m_achievements->progress();
    });
    return ok;
}

bool MainWindow::writeState(const QString& path, const QByteArray& state, const QByteArray& achievements)
{
    StateHeader header;
    header.coreId = m_core.profile().id;
    header.coreName = m_core.name();
    header.gameId = m_saves->gameId();
    header.gameName = m_saves->baseName();
    header.system = systemDef(*m_gameSystem).folder;
    header.created = QDateTime::currentDateTimeUtc();
    header.rawSize = state.size();
    header.achievements = achievements;
    QImage thumbnail = m_video->currentFrame();
    if (!thumbnail.isNull())
        thumbnail = thumbnail.scaledToWidth(kThumbnailWidth, Qt::SmoothTransformation);
    QString error;
    if (StateFile::write(path, header, state, thumbnail, &error))
        return true;
    QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
    return false;
}

void MainWindow::saveStateToSlot(int slot, bool confirm)
{
    if (!stateOperationsAvailable()) {
        if (m_emu)
            showMessage("This core does not support save states");
        return;
    }
    QString error;
    if (!m_saves->claim(&error)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        return;
    }
    saveStateToFile(m_saves->statePath(slot), confirm ? slot : -1);
}

void MainWindow::saveStateToFile(const QString& path, int confirmSlot)
{
    if (!stateOperationsAvailable())
        return;
    QString error;
    if (!m_saves->claim(&error)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        return;
    }
    if (confirmSlot >= 0 && m_settings.confirmSaveLoad && QFile::exists(path)
        && QMessageBox::question(this, "Confirm Save", QString("Overwrite existing game position in slot #%1?").arg(confirmSlot)) != QMessageBox::Yes)
        return;

    QByteArray state, progress;
    if (!captureState(&state, &error, &progress)) {
        QMessageBox::warning(this, "Xfusion", QString("Could not save the state: %1").arg(error));
        showMessage("Failed to save " + QFileInfo(path).fileName());
        return;
    }
    if (!writeState(path, state, progress)) {
        showMessage("Failed to save " + QFileInfo(path).fileName());
        return;
    }
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    showMessage(QFileInfo(path).fileName() + " saved");
}

void MainWindow::loadStateFromSlot(int slot, bool confirm)
{
    if (!stateOperationsAvailable()) {
        if (m_emu)
            showMessage("This core does not support save states");
        return;
    }
    const QString path = m_saves->statePath(slot);
    if (!QFile::exists(path)) {
        showMessage("Failed to load " + QFileInfo(path).fileName());
        return;
    }
    if (confirm && m_settings.confirmSaveLoad
        && QMessageBox::question(this, "Confirm Load", QString("Load game position from slot #%1?").arg(slot)) != QMessageBox::Yes)
        return;
    loadStateFromFile(path);
}

void MainWindow::loadOopsState()
{
    if (!stateOperationsAvailable())
        return;
    if (!QFile::exists(m_saves->oopsStatePath())) {
        QMessageBox::warning(this, "Xfusion", "No game position file available.");
        return;
    }
    loadStateFromFile(m_saves->oopsStatePath());
}

void MainWindow::loadUndoState()
{
    if (!stateOperationsAvailable())
        return;
    const QString path = m_saves->undoStatePath();
    if (!QFile::exists(path)) {
        return;
    }
    QString error;
    const auto state = StateFile::read(path, &error);
    if (!state) {
        QMessageBox::warning(this, "Xfusion", QString("Could not undo: %1").arg(error));
        return;
    }
    // Undo goes back without making a new undo point, so it can be repeated.
    applyState(state->raw, QString(), false, state->header.achievements);
}

void MainWindow::maybeSaveOops()
{
    if (!stateOperationsAvailable() || QDateTime::currentMSecsSinceEpoch() - m_lastStateActivityMs < kOopsAfterMs)
        return;
    QByteArray state, progress;
    QString error;
    if (captureState(&state, &error, &progress) && m_saves->claim(&error) && writeState(m_saves->oopsStatePath(), state, progress))
        m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::loadStateFromFile(const QString& path)
{
    if (!stateOperationsAvailable())
        return;
    QString error;
    const auto state = StateFile::read(path, &error);
    if (!state) {
        QMessageBox::warning(this, "Xfusion", QString("Could not load %1: %2").arg(QFileInfo(path).fileName(), error));
        showMessage("Failed to load " + QFileInfo(path).fileName());
        return;
    }
    if (state->header.gameId != m_saves->gameId()) {
        QMessageBox::warning(this, "Xfusion", QString("%1 belongs to a different game and was not loaded.").arg(QFileInfo(path).fileName()));
        return;
    }
    if (state->header.coreId != m_core.profile().id) {
        QMessageBox::warning(this, "Xfusion",
                             QString("%1 was saved with the core \"%2\", not this one, and cannot be loaded.").arg(QFileInfo(path).fileName(), state->header.coreId));
        return;
    }
    if (state->header.coreName != m_core.name()) {
        const auto answer = QMessageBox::question(
            this, "Xfusion",
            QString("%1 was saved with %2, but the core is now %3.\n\nStates are not guaranteed to work across core versions: "
                    "the game may glitch or crash. You can undo the load.\n\nLoad it anyway?")
                .arg(QFileInfo(path).fileName(), state->header.coreName, m_core.name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    maybeSaveOops();
    applyState(state->raw, QFileInfo(path).fileName() + " loaded", true, state->header.achievements);
}

// Puts a state into the core. A load first writes the current game to the undo file; if the core rejects the state
// it is put back as it was.
bool MainWindow::applyState(const QByteArray& state, const QString& what, bool makeUndo, const QByteArray& achievements)
{
    QByteArray before, beforeProgress;
    QString error;
    QString restoreError;
    bool loaded = false;
    bool restored = true;
    bool haveBefore = false;
    const bool paused = m_manualPause || m_focusPause || m_minimizedPause || m_dialogPause > 0;
    m_emu->withCore([&] {
        haveBefore = m_core.serialize(&before, &error);
        beforeProgress = m_achievements->progress();
        loaded = m_core.unserialize(state, &error);
        if (!loaded && haveBefore)
            restored = m_core.unserialize(before, &restoreError);
        // A paused game would keep showing the old picture; one frame draws the restored one.
        if (loaded && paused)
            m_core.run();
    });

    if (!loaded) {
        QMessageBox::warning(this, "Xfusion",
                             QString("Could not load the state: %1.\n\n%2")
                                 .arg(error, restored ? "The game was left as it was."
                                                      : "The game could not be put back as it was; reset it or reopen the game."));
        return false;
    }
    if (makeUndo && haveBefore) {
        QString claimError;
        if (m_saves->claim(&claimError))
            writeState(m_saves->undoStatePath(), before, beforeProgress);
    }
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    // The achievements go back to where they were when the state was saved; a state from before this was kept (or
    // from a game without achievements) has no progress, and then they count again from here.
    if (achievements.isEmpty() || !m_achievements->restoreProgress(achievements))
        m_achievements->reset();
    if (!what.isEmpty())
        showMessage(what);
    return true;
}

void MainWindow::showStatePreview(bool saving)
{
    if (!stateOperationsAvailable())
        return;
    StatePreviewDialog dialog(*m_saves, saving, m_currentSlot / 10, this);
    connect(&dialog, &StatePreviewDialog::slotChosen, this, [this, saving, &dialog](int slot) {
        if (saving) {
            saveStateToSlot(slot, true);
            dialog.refresh();
        } else {
            loadStateFromSlot(slot, true);
            dialog.accept();
        }
    });
    dialog.exec();
}

