#include "MainWindow.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QTimer>
#include <QStatusBar>

#include "Achievements.h"
#include "AppPaths.h"
#include "Bios.h"
#include "CoreRules.h"
#include "CoresDialog.h"
#include "DiscInfo.h"
#include "Patch.h"
#include "RomInfo.h"
#include "OsdOverlay.h"
#include "LibraryPage.h"

// ------------------------------------------------------------------------------------------------ games

void MainWindow::promptOpenGame()
{
    ++m_dialogPause;
    updatePause();
    const QString path = QFileDialog::getOpenFileName(this, "Open a game", m_lastDir, romFileDialogFilter());
    --m_dialogPause;
    updatePause();
    if (!path.isEmpty())
        openGame(path);
}

void MainWindow::openGame(const QString& path)
{
    const QString libraryId = GameSaves::fingerprint(path);
    std::optional<System> system = systemForFile(path);
    if (const auto chosen = m_library->systemChoice(libraryId))
        system = chosen; // the user said what it is
    if (!system) {
        QString why = QString("Unrecognized game format: %1").arg(QDir::toNativeSeparators(path));
        if (QFileInfo(path).suffix().compare("cue", Qt::CaseInsensitive) == 0) {
            const DiscInfo disc = readDisc(path);
            if (!disc.problems.isEmpty())
                why += "\n\n" + disc.problems.join("\n");
        }
        QMessageBox::warning(this, "Xfusion", why);
        return;
    }
    const bool isDisc = *system == System::SegaCD;
    const DiscInfo disc = isDisc ? readDisc(path) : DiscInfo();
    if (isDisc && !prepareDisc(disc))
        return;

    closeGame();
    if (!selectCore(*system, m_library->coreChoice(libraryId)))
        return;
    if (isDisc)
        warnAboutAudio(disc);
    m_lastDir = QFileInfo(path).absolutePath();
    m_romDir = m_lastDir;
    const QString playPath = preparePatchedGame(path, *system);
    if (playPath.isEmpty())
        return;
    m_libraryId = libraryId;

    const QString saveDir = AppPaths::savesDir(*system, m_romDir);
    m_core.setDirectories(AppPaths::systemDir(), saveDir);
    m_saves = std::make_unique<GameSaves>(saveDir, AppPaths::statesDir(*system, m_romDir), playPath);
    if (!m_saves->warning().isEmpty())
        QMessageBox::warning(this, "Xfusion", m_saves->warning() + "\n\nA new index will be started; the old one is kept as saves.json.bad.");

    QString error;
    const InputConfig config = m_input.config();
    std::map<std::string, std::string> coreOptions;
    const auto userOptions = m_settings.coreOptions.value(m_core.profile().id);
    for (auto it = userOptions.begin(); it != userOptions.end(); ++it)
        coreOptions[it.key().toStdString()] = it.value().toStdString();
    // A region set for this game wins over the one of Options > Emulation > Core Options.
    const CoreProfile& profile = m_core.profile();
    if (const auto region = m_library->regionChoice(libraryId); region && !profile.regionKey.empty())
        coreOptions[profile.regionKey] = profile.regionValues[static_cast<int>(*region)];
    m_core.setUserOptions(coreOptions);
    m_core.setPads(config.padType(), config.portMode() == InputConfig::PortMode::TwoControllers,
                   systemDef(*system).inputGroup == InputGroup::Genesis);
    if (!m_core.loadGame(playPath, &error)) {
        if (*system == System::SegaCD) {
            error += QString("\n\nSega CD games need the BIOS for the disc's region in:\n%1\nExpected file names: %2")
                         .arg(AppPaths::systemDir(), m_core.profile().segaCdBiosFiles.join(", "));
        }
        QMessageBox::critical(this, "Xfusion", error);
        m_saves.reset();
        return;
    }

    m_gamePath = path;
    m_gameSystem = system;
    applyView();
    m_lastSaveRam.clear();
    m_saveRamLocked = false;
    m_saveWriteFailing = false;
    m_statesSupported = m_core.supportsStates();
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    m_manualPause = false;
    m_fastForwardHeld = false;
    m_fastForwardToggled = false;
    loadSaveRam();
    const qsizetype sramSize = m_core.saveRam().size();
    m_discSet.clear();
    if (isDisc)
        setupDiscs(path);
    loadAchievements(*system, path, isDisc);

    m_audio = std::make_unique<AudioOutput>(m_core.avInfo().sampleRate, m_view.audioDevice, m_view.playbackRate);
    m_inputGroup = systemDef(*system).inputGroup;
    m_emu = std::make_unique<EmuThread>(m_core, m_audio.get(), [this] { pollInput(); });
    m_emu->setFrameHook([this] { m_achievements->frame(); });
    m_emu->setRewind(static_cast<std::size_t>(m_settings.rewindMegabytes) * 1024 * 1024, m_settings.rewindInterval);
    updateSpeed();
    updateAudioMode();
    m_emu->start();
    if (m_settings.sramInterval > 0)
        m_saveTimer.start(m_settings.sramInterval * 1000);

    m_central->setCurrentWidget(m_video);
    addRecent(path);
    setWindowTitle(QString("Xfusion - %1").arg(QFileInfo(path).completeBaseName()));
    updateActions();
    updatePause();
    setFocus();

    if (m_settings.showRomInfo)
        showMessage(startText(path, playPath, *system, disc, sramSize));
    if (!m_coreNote.isEmpty())
        showMessage(m_coreNote);
    if (m_settings.fullscreenOnOpen && !isFullScreen())
        toggleFullscreen();
    if (m_settings.saveStateOnClose && m_statesSupported && QFile::exists(m_saves->resumeStatePath()))
        loadStateFromFile(m_saves->resumeStatePath());
}

// The message snes9xrd shows for a ROM, for this game (RomInfo.h).
QString MainWindow::startText(const QString& path, const QString& playPath, System system, const DiscInfo& disc, qsizetype sramSize) const
{
    StartInfo info;
    info.system = systemDef(system).folder;
    info.core = coreInfo(m_core.profile().id) ? coreInfo(m_core.profile().id)->name : m_core.name();
    info.pal = m_core.avInfo().fps < 55;
    info.sram = sramSize <= 0 ? QString("none") : sramSize >= 1024 ? QString("%1KB").arg(sramSize / 1024) : QString("%1B").arg(sramSize);
    info.patch = m_patchKind;
    GameHeader header;
    if (system == System::SegaCD) {
        header = disc.header;
        info.size = QString("%1 data / %2 audio %3").arg(disc.dataTracks).arg(disc.audioTracks).arg(disc.audioTracks == 1 ? "track" : "tracks");
    } else {
        QByteArray data;
        QString extension, error;
        if (readGameBytes(playPath, &data, &extension, &error)) {
            header = parseCartridgeHeader(data.left(0x8000), extension);
            info.size = sizeInBits(data.size());
            info.hasCrc = true;
            info.crc = crc32Of(data);
            info.checksum = cartridgeChecksum(data);
        }
    }
    info.title = header.title().isEmpty() ? QFileInfo(path).completeBaseName() : header.title();
    info.product = header.product;
    info.revision = revisionOfProduct(header.product);
    return startMessage(info);
}

// Looks at a Sega CD disc before the core is asked to load it, so that what is wrong is said in plain words: tracks
// missing from the cue sheet, or no BIOS for the region of the disc.
bool MainWindow::prepareDisc(const DiscInfo& disc)
{
    if (!disc.problems.isEmpty())
        QMessageBox::warning(this, "Xfusion", "Some files of this disc are missing, so it may lack its music:\n\n" + disc.problems.join("\n"));

    const unsigned regions = disc.header.regions;
    if (regions == 0)
        return true; // a CHD that libchdr cannot read, or a header that does not say: the core decides
    const auto offered = [&](const BiosStatus& status) {
        return ((regions & GameHeader::Japan) && status.japan) || ((regions & GameHeader::Americas) && status.usa) || ((regions & GameHeader::Europe) && status.europe);
    };
    BiosStatus status = checkBios(AppPaths::systemDir());
    if (offered(status))
        return true;

    QStringList names;
    if (regions & GameHeader::Japan)
        names << biosFileName(BiosRegion::Japan);
    if (regions & GameHeader::Americas)
        names << biosFileName(BiosRegion::USA);
    if (regions & GameHeader::Europe)
        names << biosFileName(BiosRegion::Europe);
    if (names.isEmpty())
        return true; // an Asia-only disc: no BIOS of its own to ask for
    QMessageBox box(QMessageBox::Warning, "Xfusion",
                    QString("This disc is for %1 and needs the Sega CD BIOS for that region: %2.\n\nThey go in %3.\n\n"
                            "If you have BIOS dumps (with any file name), Xfusion can find them and copy them there.")
                        .arg(disc.header.regionText(), names.join(" or "), QDir::toNativeSeparators(AppPaths::systemDir())),
                    QMessageBox::NoButton, this);
    QPushButton* find = box.addButton("Find BIOS Files...", QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != find)
        return false;
    const QString folder = QFileDialog::getExistingDirectory(this, "Folder with the BIOS dumps", m_lastDir);
    if (folder.isEmpty())
        return false;
    QString error;
    const QStringList installed = installBios(folder, AppPaths::systemDir(), &error);
    if (!error.isEmpty())
        QMessageBox::warning(this, "Xfusion", error);
    status = checkBios(AppPaths::systemDir());
    if (!installed.isEmpty())
        showMessage("BIOS installed: " + installed.join(", "));
    if (!offered(status)) {
        QMessageBox::warning(this, "Xfusion", "No BIOS for this disc was found in that folder.");
        return false;
    }
    return true;
}

// Neither core plays every .mp3: Genesis Plus GX none (WAV and OGG only), PicoDrive only MPEG-1 at 44.1 kHz, and
// it skips the others, which shifts the numbers of the tracks after them. Said when the disc is opened.
void MainWindow::warnAboutAudio(const DiscInfo& disc)
{
    int mp3 = 0;
    for (const QString& file : disc.audioFiles)
        if (file.endsWith(".mp3", Qt::CaseInsensitive))
            ++mp3;
    if (mp3 == 0)
        return;
    if (m_core.profile().id == CoreId::GenesisPlusGx) {
        const QString note = QString("%1 of %2 audio tracks are .mp3, which Genesis Plus GX cannot play (WAV or OGG only)").arg(mp3).arg(disc.audioFiles.size());
        m_coreNote = m_coreNote.isEmpty() ? note : m_coreNote + "; " + note;
    } else if (!disc.unplayableMp3.isEmpty()) {
        QMessageBox::warning(this, "Xfusion",
                             QString("%1 of the %2 audio tracks of this disc are .mp3 files in a format PicoDrive cannot play (it plays MPEG-1 at "
                                     "44.1 kHz only; for example %3 is not).\n\nThe music will be missing, and the tracks after each unplayable one "
                                     "will be shifted, so the game may play the wrong music.\n\nConvert the tracks, for instance with ffmpeg: "
                                     "ffmpeg -i \"in.mp3\" -ar 44100 \"out.mp3\".")
                                 .arg(disc.unplayableMp3.size()).arg(disc.audioFiles.size()).arg(disc.unplayableMp3.first()));
    }
}

void MainWindow::offerCores(const QString& reason)
{
    QTimer::singleShot(0, this, [this, reason] {
        QMessageBox box(QMessageBox::Information, "Xfusion",
                        (reason.isEmpty() ? QString("No libretro core is installed.") : reason) +
                            QString("\n\nXfusion plays games with libretro cores, which are downloaded separately (Genesis Plus GX and PicoDrive, each with its own "
                                    "license). They are kept in %1.\n\nOpen Manage Cores to download them?")
                                .arg(QDir::toNativeSeparators(AppPaths::coresDir())),
                        QMessageBox::NoButton, this);
        QPushButton* manage = box.addButton("Manage Cores...", QMessageBox::AcceptRole);
        box.addButton("Later", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == manage)
            CoresDialog(this).exec();
    });
}

// Chooses the core for the game (CoreRules) and loads it when it is not the one already loaded.
bool MainWindow::selectCore(System system, const QString& gameCoreChoice)
{
    m_coreNote.clear();
    if (m_coreForced)
        return true;
    const CoreDecision decision = chooseCore(system, m_settings.coreChoices, [](const QString& id) { return AppPaths::coreInstalled(id); }, gameCoreChoice);
    if (decision.id.isEmpty()) {
        offerCores(QString("No installed core can play %1 games.").arg(systemDef(system).folder));
        return false;
    }
    QString error;
    if (!m_core.useCore(coreProfileById(decision.id), AppPaths::corePath(decision.id), &error)) {
        QMessageBox::critical(this, "Xfusion", error);
        return false;
    }
    m_coreNote = decision.note;
    return true;
}

QString MainWindow::preparePatchedGame(const QString& path, System system)
{
    m_patchedGame.reset();
    m_patchName.clear();
    m_patchKind.clear();
    const QString patchFile = findPatch(path, AppPaths::patchFolders(system, QFileInfo(path).absolutePath()));
    if (patchFile.isEmpty())
        return path;
    if (system == System::SegaCD) {
        // A disc is many files, or a compressed one: there is no single file to patch. Say so instead of ignoring it.
        const bool chd = QFileInfo(path).suffix().compare("chd", Qt::CaseInsensitive) == 0;
        const auto answer = QMessageBox::warning(
            this, "Xfusion",
            QString("There is a patch for this game (%1), but patches cannot be applied to Sega CD discs%2.\n\nOpen the game without it?")
                .arg(QFileInfo(patchFile).fileName(),
                     chd ? " (a .chd is compressed, so its bytes are not where the patch expects them)" : QString()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        return answer == QMessageBox::Yes ? path : QString();
    }

    const QString patchName = QFileInfo(patchFile).fileName();
    QByteArray game, patch;
    QString extension, error;
    PatchResult result;
    QFile file(patchFile);
    if (!readGameBytes(path, &game, &extension, &error)) {
        // Nothing to patch; the game will report its own problem when it loads.
        return path;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        error = QString("%1 could not be read").arg(patchName);
    } else {
        patch = file.readAll();
        result = applyPatch(game, patch);
        if (result.ok)
            error.clear();
        else
            error = result.error;
    }
    if (!error.isEmpty()) {
        const auto answer = QMessageBox::warning(this, "Xfusion",
                                                 QString("The patch %1 could not be applied to %2:\n\n%3\n\nOpen the game without the patch?")
                                                     .arg(patchName, QFileInfo(path).fileName(), error),
                                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        return answer == QMessageBox::Yes ? path : QString();
    }

    // The patched copy is named after the game and the patch, so that its saves and states are its own.
    m_patchedGame = std::make_unique<QTemporaryDir>();
    QString target = m_patchedGame->filePath(QString("%1 [%2].%3").arg(QFileInfo(path).completeBaseName(), QFileInfo(patchFile).completeBaseName(), extension));
    QFile out(target);
    if (!m_patchedGame->isValid() || !out.open(QIODevice::WriteOnly) || out.write(result.data) != result.data.size()) {
        QMessageBox::warning(this, "Xfusion", "A temporary file for the patched game could not be written.");
        m_patchedGame.reset();
        return QString();
    }
    m_patchName = patchName;
    m_patchKind = result.format == PatchFormat::Ips ? "IPS" : result.format == PatchFormat::Bps ? "BPS" : "UPS";
    return target;
}

bool MainWindow::closeGame()
{
    if (!m_core.hasGame())
        return true;

    // The resume state is taken while the core is still running; the thread is stopped right after.
    if (m_settings.saveStateOnClose && m_statesSupported && m_emu) {
        QByteArray state;
        QString error;
        QByteArray progress;
        if (captureState(&state, &error, &progress)) {
            if (!m_saves->claim(&error) || !writeState(m_saves->resumeStatePath(), state, progress))
                QMessageBox::warning(this, "Xfusion", "Could not save the state for the next time: " + error);
        }
    }

    trackPlayTime(false);
    commitPlayTime();
    m_saveTimer.stop();
    m_emu.reset();
    flushSaveRam(true);
    m_achievements->unloadGame();
    m_achievementsTried = false;
    m_core.unloadGame();
    m_discSet.clear();
    m_patchedGame.reset();
    m_audio.reset();
    m_saves.reset();
    m_gameSystem.reset();
    applyView();
    m_statesSupported = false;
    m_input.setSwapped(false);
    m_manualPause = false;
    m_video->clear();
    m_video->osd()->setIndicators(false, false, false);
    m_central->setCurrentWidget(m_library);
    m_library->focusList();
    setWindowTitle("Xfusion");
    updateActions();
    return true;
}

void MainWindow::resetGame(bool powerCycle)
{
    if (!m_emu)
        return;
    maybeSaveOops();
    m_lastStateActivityMs = QDateTime::currentMSecsSinceEpoch();
    if (powerCycle) {
        // Power cycling is closing and opening the game again: the core is reloaded and the saves are kept.
        const QString path = m_gamePath;
        openGame(path);
        showMessage("Power cycle");
        return;
    }
    m_emu->withCore([this] {
        m_core.reset();
        m_achievements->reset();
    });
    if (m_manualPause)
        setManualPause(false);
    showMessage("Reset");
}

void MainWindow::showRomInfo()
{
    if (!m_saves || !m_gameSystem)
        return;
    const QFileInfo info(m_gamePath);
    GameHeader header;
    QString extra;
    if (*m_gameSystem == System::SegaCD) {
        const DiscInfo disc = readDisc(m_gamePath);
        header = disc.header;
        extra = QString("Disc: %1, %2 data / %3 audio tracks\n").arg(disc.format).arg(disc.dataTracks).arg(disc.audioTracks);
        if (!disc.problems.isEmpty())
            extra += "Problems: " + disc.problems.join("; ") + "\n";
    } else {
        header = readCartridgeHeader(m_gamePath);
    }
    if (!m_patchName.isEmpty())
        extra += "Patch applied: " + m_patchName + "\n";
    if (header.valid) {
        extra += QString("Name in game: %1\nProduct: %2\nRegion: %3\n").arg(header.title(), header.product, header.regionText().isEmpty() ? QString("not stated") : header.regionText());
    }
    QMessageBox::information(this, "ROM Information",
                             extra + QString("Title: %1\nFile: %2\nSize: %3 bytes\nSystem: %4\nCore: %5\nGame id: %6\nSaves: %7\nStates: %8")
                                 .arg(info.completeBaseName(), QDir::toNativeSeparators(m_gamePath))
                                 .arg(info.size())
                                 .arg(systemDef(*m_gameSystem).folder, m_core.name(), m_saves->gameId(),
                                      QDir::toNativeSeparators(m_saves->saveRamPath()), m_statesSupported ? "supported" : "not supported by this core"));
}

void MainWindow::takeScreenshot()
{
    if (!m_emu || !m_gameSystem || !m_saves)
        return;
    const QImage frame = m_video->currentFrame();
    if (frame.isNull()) {
        showMessage("There is no picture to save yet");
        return;
    }
    const QString folder = AppPaths::screenshotsDir(*m_gameSystem, m_romDir);
    if (!QDir().mkpath(folder)) {
        QMessageBox::warning(this, "Xfusion", QString("Failed to take screenshot: cannot create %1").arg(folder));
        return;
    }
    const QString stem = QString("%1-%2").arg(m_saves->baseName(), QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss"));
    QString path = folder + "/" + stem + ".png";
    for (int i = 0; QFile::exists(path) && i < 1000; ++i)
        path = QString("%1/%2-%3.png").arg(folder, stem).arg(i);
    if (!frame.save(path, "PNG")) {
        QMessageBox::warning(this, "Xfusion", QString("Failed to take screenshot: %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    showMessage(QString("Saved screenshot %1").arg(QFileInfo(path).fileName()));
}

// A save file that cannot be used as it is stays untouched for the whole session: writing the core's own RAM
// over it at exit would destroy the only copy.
void MainWindow::loadSaveRam()
{
    QByteArray saved;
    QString error;
    switch (m_saves->readSaveRam(&saved, &error)) {
    case GameSaves::ReadResult::Missing:
        return;
    case GameSaves::ReadResult::Failed:
        m_saveRamLocked = true;
        QMessageBox::warning(this, "Xfusion", error + "\n\nThe game starts without its save and the file will not be overwritten in this session.");
        return;
    case GameSaves::ReadResult::Loaded:
        break;
    }

    QString warning;
    const bool loaded = m_core.setSaveRam(saved, &warning);
    if (loaded && warning.isEmpty()) {
        const qsizetype expected = m_core.profile().saveRamSizeVaries ? saved.size() : m_core.saveRam().size();
        if (expected > saved.size()) {
            // A save from another emulator that stores less. Keep the original next to it, once, before the first
            // write grows the file to the size this core uses.
            const QString original = m_saves->saveRamPath() + ".orig";
            if (!QFile::exists(original))
                QFile::copy(m_saves->saveRamPath(), original);
            showMessage(QString("Save file had %1 of %2 bytes; the rest starts empty (original kept as %3)")
                            .arg(saved.size()).arg(expected).arg(QFileInfo(original).fileName()));
            return; // m_lastSaveRam stays empty, so the full-size save is written at the next flush
        }
        m_lastSaveRam = saved;
        return;
    }
    m_saveRamLocked = true;
    QMessageBox::warning(this, "Xfusion",
                         QString("%1 was not loaded: %2.\n\nThe file will not be overwritten in this session.").arg(m_saves->saveRamPath(), warning));
}

// Writes the battery RAM when it changed. Closing the game always reports a failure; while playing, the first
// failure is reported and the write is retried quietly until it works.
void MainWindow::flushSaveRam(bool closing)
{
    if (!m_saves || m_saveRamLocked)
        return;

    QByteArray current;
    if (m_emu)
        m_emu->withCore([&] { current = m_core.saveRam(); });
    else
        current = m_core.saveRam();
    // PicoDrive reports no battery RAM while it is all zero, so a game that clears its own save looks like "none".
    // Once a save has existed, an empty report means it was cleared.
    if (current.isEmpty() && !m_lastSaveRam.isEmpty())
        current = QByteArray(m_lastSaveRam.size(), m_core.profile().saveRamSizeVaries ? char(0xFF) : char(0)); // Genesis Plus GX clears to 0xFF
    if (current.isEmpty() || current == m_lastSaveRam)
        return;

    QString error;
    if (m_saves->writeSaveRam(current, &error)) {
        m_lastSaveRam = current;
        m_saveWriteFailing = false;
        return;
    }
    if (closing || !m_saveWriteFailing) {
        QMessageBox::warning(this, "Xfusion",
                             error + (closing ? "\n\nThe progress since the last successful save is lost."
                                              : "\n\nXfusion will keep trying every few seconds."));
    }
    m_saveWriteFailing = true;
}

void MainWindow::saveSramNow()
{
    if (!m_emu || !m_saves)
        return;
    flushSaveRam(false);
    if (QFile::exists(m_saves->saveRamPath()))
        showMessage(QString("Saved S-RAM %1").arg(QFileInfo(m_saves->saveRamPath()).fileName()));
    else
        showMessage("This game has nothing to save in S-RAM yet");
}

