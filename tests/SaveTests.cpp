#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include "GameSaves.h"
#include "StateFile.h"
#include "SystemInfo.h"

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(bytes);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

}

class SaveTests : public QObject {
    Q_OBJECT

private slots:
    void sanitizedNamesAreSafeOnEveryFilesystem()
    {
        QCOMPARE(GameSaves::sanitizedName("C:/roms/Sonic The Hedgehog (W) [!].md"), QString("Sonic The Hedgehog (W) [!]"));
        QCOMPARE(GameSaves::sanitizedName("C:/roms/a<b>c:d\"e|f?g*h.md"), QString("a_b_c_d_e_f_g_h"));
        QCOMPARE(GameSaves::sanitizedName("C:/roms/Ends with dot..md"), QString("Ends with dot"));
        QCOMPARE(GameSaves::sanitizedName("C:/roms/CON.md"), QString("_CON"));
        QCOMPARE(GameSaves::sanitizedName("C:/roms/lpt1.md"), QString("_lpt1"));
        QCOMPARE(GameSaves::sanitizedName("C:/roms/.md"), QString("game"));
        QVERIFY(GameSaves::sanitizedName("C:/roms/" + QString(300, 'x') + ".md").size() <= 120);
    }

    void everySystemHasItsOwnSaveFolder()
    {
        QSet<QString> folders;
        for (const SystemDef& def : allSystems()) {
            QVERIFY(!def.folder.isEmpty());
            QVERIFY2(!folders.contains(def.folder.toLower()), qPrintable(def.folder));
            folders.insert(def.folder.toLower());
        }
    }

    void theFingerprintFollowsTheContentNotThePath()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a/Game.md"), "same content");
        writeFile(dir.filePath("b/Renamed.md"), "same content");
        writeFile(dir.filePath("c/Game.md"), "other content");
        const QString a = GameSaves::fingerprint(dir.filePath("a/Game.md"));
        QVERIFY(!a.isEmpty());
        QCOMPARE(GameSaves::fingerprint(dir.filePath("b/Renamed.md")), a);
        QVERIFY(GameSaves::fingerprint(dir.filePath("c/Game.md")) != a);
        QVERIFY(GameSaves::fingerprint(dir.filePath("missing.md")).isEmpty());
    }

    void aLargeFileIsToldApartByItsEnds()
    {
        QTemporaryDir dir;
        QByteArray big(3 * 1024 * 1024, 'a');
        writeFile(dir.filePath("one.iso"), big);
        big[big.size() - 10] = 'b'; // changes only the tail
        writeFile(dir.filePath("two.iso"), big);
        QVERIFY(GameSaves::fingerprint(dir.filePath("one.iso")) != GameSaves::fingerprint(dir.filePath("two.iso")));
    }

    void cueSheetsCountTheFilesTheyPointAt()
    {
        QTemporaryDir dir;
        const QByteArray sheet = "FILE \"disc.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n";
        writeFile(dir.filePath("one/Game.cue"), sheet);
        writeFile(dir.filePath("one/disc.bin"), "first disc");
        writeFile(dir.filePath("two/Game.cue"), sheet);
        writeFile(dir.filePath("two/disc.bin"), "second disc");
        QVERIFY(GameSaves::fingerprint(dir.filePath("one/Game.cue")) != GameSaves::fingerprint(dir.filePath("two/Game.cue")));
    }

    void aFirstGameKeepsItsPlainName()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Sonic.md"), "sonic");
        GameSaves saves(dir.filePath("saves/Genesis"), dir.filePath("states/Genesis"), dir.filePath("roms/Sonic.md"));
        QCOMPARE(saves.baseName(), QString("Sonic"));
        QCOMPARE(saves.saveRamPath(), dir.filePath("saves/Genesis/Sonic.srm"));
        QCOMPARE(saves.statePath(0), dir.filePath("states/Genesis/Sonic.000.state"));
        QCOMPARE(saves.statePath(9), dir.filePath("states/Genesis/Sonic.009.state"));
        QCOMPARE(saves.statePath(123), dir.filePath("states/Genesis/Sonic.123.state"));
        QCOMPARE(saves.statePath(999), dir.filePath("states/Genesis/Sonic.999.state"));
        QCOMPARE(saves.undoStatePath(), dir.filePath("states/Genesis/Sonic.undo.state"));
        QCOMPARE(saves.oopsStatePath(), dir.filePath("states/Genesis/Sonic.oops.state"));
        QCOMPARE(saves.resumeStatePath(), dir.filePath("states/Genesis/Sonic.resume.state"));

        QByteArray data;
        QString error;
        QCOMPARE(saves.readSaveRam(&data, &error), GameSaves::ReadResult::Missing);
        QVERIFY2(saves.writeSaveRam("progress", &error), qPrintable(error));
        QCOMPARE(saves.readSaveRam(&data, &error), GameSaves::ReadResult::Loaded);
        QCOMPARE(data, QByteArray("progress"));
    }

    void twoGamesWithTheSameFileNameDoNotOverwriteEachOther()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("hacks/Sonic.md"), "a hack of sonic");
        writeFile(dir.filePath("roms/Sonic.md"), "the original sonic");
        const QString saves = dir.filePath("saves/Genesis");
        const QString states = dir.filePath("states/Genesis");
        QString error;

        GameSaves first(saves, states, dir.filePath("roms/Sonic.md"));
        QVERIFY2(first.writeSaveRam("original progress", &error), qPrintable(error));

        GameSaves second(saves, states, dir.filePath("hacks/Sonic.md"));
        QVERIFY(second.baseName() != first.baseName());
        QVERIFY2(second.baseName().startsWith("Sonic ("), qPrintable(second.baseName()));
        QVERIFY2(second.writeSaveRam("hack progress", &error), qPrintable(error));
        QVERIFY(second.saveRamPath() != first.saveRamPath());
        QVERIFY(second.statePath(0) != first.statePath(0));

        QCOMPARE(readFile(first.saveRamPath()), QByteArray("original progress"));
        QCOMPARE(readFile(second.saveRamPath()), QByteArray("hack progress"));

        // Opening them again finds each one's own file.
        QCOMPARE(GameSaves(saves, states, dir.filePath("roms/Sonic.md")).baseName(), first.baseName());
        QCOMPARE(GameSaves(saves, states, dir.filePath("hacks/Sonic.md")).baseName(), second.baseName());
    }

    void namesThatDifferOnlyInCaseAreTheSameOnWindows()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a/SONIC.md"), "one");
        writeFile(dir.filePath("b/sonic.md"), "two");
        QString error;
        GameSaves first(dir.filePath("s"), dir.filePath("t"), dir.filePath("a/SONIC.md"));
        QVERIFY(first.writeSaveRam("1", &error));
        GameSaves second(dir.filePath("s"), dir.filePath("t"), dir.filePath("b/sonic.md"));
        QVERIFY(second.baseName().compare(first.baseName(), Qt::CaseInsensitive) != 0);
    }

    void aGameKeepsItsSavesWhenItsFileIsRenamedOrMoved()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Sonic.md"), "sonic");
        writeFile(dir.filePath("elsewhere/Sonic the Hedgehog.md"), "sonic");
        QString error;
        GameSaves first(dir.filePath("s"), dir.filePath("t"), dir.filePath("roms/Sonic.md"));
        QVERIFY(first.writeSaveRam("kept", &error));

        GameSaves moved(dir.filePath("s"), dir.filePath("t"), dir.filePath("elsewhere/Sonic the Hedgehog.md"));
        QCOMPARE(moved.baseName(), QString("Sonic"));
        QByteArray data;
        QCOMPARE(moved.readSaveRam(&data, &error), GameSaves::ReadResult::Loaded);
        QCOMPARE(data, QByteArray("kept"));
    }

    void gamesOfDifferentSystemsNeverShareAFile()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Columns.md"), "genesis columns");
        writeFile(dir.filePath("roms/Columns.gg"), "game gear columns");
        QString error;
        GameSaves genesis(dir.filePath("saves/Genesis"), dir.filePath("states/Genesis"), dir.filePath("roms/Columns.md"));
        GameSaves gameGear(dir.filePath("saves/Game Gear"), dir.filePath("states/Game Gear"), dir.filePath("roms/Columns.gg"));
        QCOMPARE(genesis.baseName(), gameGear.baseName()); // same name is fine: the folders differ
        QVERIFY(genesis.writeSaveRam("g", &error));
        QVERIFY(gameGear.writeSaveRam("gg", &error));
        QVERIFY(genesis.saveRamPath() != gameGear.saveRamPath());
        QCOMPARE(readFile(genesis.saveRamPath()), QByteArray("g"));
        QCOMPARE(readFile(gameGear.saveRamPath()), QByteArray("gg"));
    }

    void aSaveThatPredatesTheIndexBelongsToTheGameWithThatName()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Old.md"), "old");
        writeFile(dir.filePath("s/Old.srm"), "from an older version");
        GameSaves saves(dir.filePath("s"), dir.filePath("t"), dir.filePath("roms/Old.md"));
        QCOMPARE(saves.baseName(), QString("Old"));
        QByteArray data;
        QString error;
        QCOMPARE(saves.readSaveRam(&data, &error), GameSaves::ReadResult::Loaded);
        QCOMPARE(data, QByteArray("from an older version"));
    }

    void anUnreadableIndexIsKeptAndRebuilt()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Game.md"), "game");
        writeFile(dir.filePath("s/saves.json"), "{ not json");
        GameSaves saves(dir.filePath("s"), dir.filePath("t"), dir.filePath("roms/Game.md"));
        QVERIFY(!saves.warning().isEmpty());
        QString error;
        QVERIFY2(saves.writeSaveRam("x", &error), qPrintable(error));
        QCOMPARE(readFile(dir.filePath("s/saves.json.bad")), QByteArray("{ not json"));
        QVERIFY(QJsonDocument::fromJson(readFile(dir.filePath("s/saves.json"))).isObject());
        QVERIFY(GameSaves(dir.filePath("s"), dir.filePath("t"), dir.filePath("roms/Game.md")).warning().isEmpty());
    }

    void aFailedWriteIsReportedNotHidden()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Game.md"), "game");
        writeFile(dir.filePath("blocked"), "this is a file, not a folder");
        GameSaves saves(dir.filePath("blocked/saves"), dir.filePath("t"), dir.filePath("roms/Game.md"));
        QString error;
        QVERIFY(!saves.writeSaveRam("x", &error));
        QVERIFY(!error.isEmpty());
    }

    void foldersAreCreatedOnlyWhenSomethingIsWritten()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("roms/Game.md"), "game");
        GameSaves saves(dir.filePath("saves/Genesis"), dir.filePath("states/Genesis"), dir.filePath("roms/Game.md"));
        QVERIFY(!QDir(dir.filePath("saves")).exists());
        QVERIFY(!QDir(dir.filePath("states")).exists());
        QString error;
        QVERIFY(saves.writeSaveRam("x", &error));
        QVERIFY(QDir(dir.filePath("saves/Genesis")).exists());
    }

    void aStateSurvivesTheRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("states/Genesis/Game.000.state");
        QByteArray raw(200000, 'x');
        for (int i = 0; i < raw.size(); i += 7)
            raw[i] = static_cast<char>(i);

        StateHeader header;
        header.coreId = "picodrive";
        header.coreName = "PicoDrive 2.04";
        header.gameId = "abc123";
        header.gameName = "Game";
        header.system = "Genesis";
        header.created = QDateTime(QDate(2026, 11, 19), QTime(18, 30), QTimeZone::utc());
        header.rawSize = raw.size();

        QString error;
        QVERIFY2(StateFile::write(path, header, raw, QImage(), &error), qPrintable(error));
        const auto loaded = StateFile::read(path, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->raw, raw);
        QCOMPARE(loaded->header.coreId, header.coreId);
        QCOMPARE(loaded->header.coreName, header.coreName);
        QCOMPARE(loaded->header.gameId, header.gameId);
        QCOMPARE(loaded->header.system, header.system);
        QCOMPARE(loaded->header.created, header.created);

        const auto onlyHeader = StateFile::readHeader(path);
        QVERIFY(onlyHeader.has_value());
        QCOMPARE(onlyHeader->coreName, header.coreName);
    }

    void aStateKeepsItsThumbnailAndFormatOneStillLoads()
    {
        QTemporaryDir dir;
        StateHeader header;
        header.coreId = "picodrive";
        header.gameId = "id";
        QImage picture(32, 24, QImage::Format_RGB32);
        picture.fill(QColor(10, 200, 30));

        const QString path = dir.filePath("with.state");
        QString error;
        QVERIFY(StateFile::write(path, header, QByteArray("payload"), picture, &error));
        QImage thumbnail;
        QVERIFY(StateFile::readHeader(path, &thumbnail).has_value());
        QCOMPARE(thumbnail.size(), picture.size());
        QCOMPARE(QColor(thumbnail.pixel(5, 5)), QColor(10, 200, 30));
        const auto loaded = StateFile::read(path, &error);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->thumbnail.size(), picture.size());
        QCOMPARE(loaded->raw, QByteArray("payload"));

        // The same file without a picture, turned into the old format 1 layout (no thumbnail block).
        const QString plain = dir.filePath("plain.state");
        QVERIFY(StateFile::write(plain, header, QByteArray("old"), QImage(), &error));
        QByteArray bytes = readFile(plain);
        const int headerSize = (quint8(bytes[8]) << 24) | (quint8(bytes[9]) << 16) | (quint8(bytes[10]) << 8) | quint8(bytes[11]);
        bytes.remove(12 + headerSize, 4); // the zero-length thumbnail
        bytes[7] = 1;
        const QString old = dir.filePath("old.state");
        writeFile(old, bytes);
        const auto legacy = StateFile::read(old, &error);
        QVERIFY2(legacy.has_value(), qPrintable(error));
        QCOMPARE(legacy->raw, QByteArray("old"));
        QVERIFY(legacy->thumbnail.isNull());
    }

    void theAchievementProgressTravelsWithTheState()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("with.state");
        StateHeader header;
        header.coreId = "genesis_plus_gx";
        header.gameId = "game";
        header.rawSize = 4;
        QByteArray progress(3000, 0);
        for (int i = 0; i < progress.size(); ++i)
            progress[i] = char(i * 7); // bytes of any value
        header.achievements = progress;
        QString error;
        QVERIFY2(StateFile::write(path, header, "raw!", QImage(), &error), qPrintable(error));
        const auto loaded = StateFile::read(path, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->header.achievements, progress);
        QCOMPARE(StateFile::readHeader(path)->achievements, progress);

        // A state saved without achievements has none to restore.
        header.achievements.clear();
        QVERIFY(StateFile::write(dir.filePath("without.state"), header, "raw!", QImage(), &error));
        QVERIFY(StateFile::read(dir.filePath("without.state"), &error)->header.achievements.isEmpty());
    }

    void savingOverAStateKeepsTheOldOneAsABackup()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Game.000.state");
        StateHeader header;
        header.coreId = "picodrive";
        header.gameId = "id";
        QString error;
        QVERIFY(StateFile::write(path, header, QByteArray("first"), QImage(), &error));
        QVERIFY(StateFile::write(path, header, QByteArray("second"), QImage(), &error));
        QCOMPARE(StateFile::read(path, &error)->raw, QByteArray("second"));
        QCOMPARE(StateFile::read(path + ".bak", &error)->raw, QByteArray("first"));
    }

    void damagedStatesAreRefusedWithAReason()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Game.000.state");
        StateHeader header;
        header.gameId = "id";
        QString error;
        QVERIFY(StateFile::write(path, header, QByteArray(5000, 'q'), QImage(), &error));
        const QByteArray good = readFile(path);

        QVERIFY(!StateFile::read(dir.filePath("missing.state"), &error).has_value());
        QVERIFY(!error.isEmpty());

        error.clear();
        writeFile(path, good.left(good.size() / 2)); // cut short
        QVERIFY(!StateFile::read(path, &error).has_value());
        QVERIFY(!error.isEmpty());

        error.clear();
        QByteArray flipped = good;
        flipped[flipped.size() - 3] = static_cast<char>(flipped[flipped.size() - 3] ^ 0x5A);
        writeFile(path, flipped);
        QVERIFY(!StateFile::read(path, &error).has_value());
        QVERIFY(!error.isEmpty());

        error.clear();
        writeFile(path, "not a state at all, just text");
        QVERIFY(!StateFile::read(path, &error).has_value());
        QVERIFY(error.contains("not a Xfusion save state"));
        QVERIFY(!StateFile::readHeader(path).has_value());

        error.clear();
        QByteArray newer = good;
        newer[7] = 9; // format version 9
        writeFile(path, newer);
        QVERIFY(!StateFile::read(path, &error).has_value());
        QVERIFY(error.contains("newer"));
    }
};

QTEST_APPLESS_MAIN(SaveTests)
#include "SaveTests.moc"
